#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>

// 字幕序号的静态变量定义
static int subtitleNumber = 1;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("视频字幕提取工具");
    
    forceStop = false;
    // 设置配置文件路径
    configFilePath = getAppPath() + "config.json";
    
    // 加载配置
    loadConfig();
    
    // 应用配置
    ui->srtCheckBox->setChecked(config.srtEnabled);
    ui->txtCheckBox->setChecked(config.txtEnabled);
    
    ffmpegProcess = new QProcess(this);
    wav2srtProcess = new QProcess(this);
    getVideoDurationProcess = new QProcess(this);
    
    // 连接FFmpeg进程信号
    connect(ffmpegProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::ffmpegReadyReadStandardOutput);
    connect(ffmpegProcess, &QProcess::readyReadStandardError, this, &MainWindow::ffmpegReadyReadStandardError);
    connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::ffmpegFinished);
    
    // 连接wav2srt进程信号
    connect(wav2srtProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::wav2srtReadyReadStandardOutput);
    connect(wav2srtProcess, &QProcess::readyReadStandardError, this, &MainWindow::wav2srtReadyReadStandardError);
    connect(wav2srtProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::wav2srtFinished);
    
    // 连接获取视频时长进程信号
    connect(getVideoDurationProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::getVideoDurationReadyReadStandardOutput);
    connect(getVideoDurationProcess, &QProcess::readyReadStandardError, this, &MainWindow::getVideoDurationReadyReadStandardError);
    connect(getVideoDurationProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::getVideoDurationFinished);
    
    // 连接配置变化信号
    connect(ui->srtCheckBox, SIGNAL(stateChanged(int)), this, SLOT(on_srtCheckBox_stateChanged(int)));
    connect(ui->txtCheckBox, SIGNAL(stateChanged(int)), this, SLOT(on_txtCheckBox_stateChanged(int)));
    
    // 初始化UI状态
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(false);
    ui->statusLabel->setText("请选择视频文件");
    ui->progressBar->setValue(0);
    
    // 启用拖放
    setAcceptDrops(true);
    
    // 初始化处理状态
    isProcessing = false;
}

MainWindow::~MainWindow()
{
    // 保存配置
    saveConfig();
    
    delete ui;
}

void MainWindow::loadConfig()
{
    QFile file(configFilePath);
    
    // 设置默认配置
    config.srtEnabled = true;
    config.txtEnabled = true;
    config.lastVideoDir = "";
    
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            
            if (obj.contains("srtEnabled") && obj["srtEnabled"].isBool())
                config.srtEnabled = obj["srtEnabled"].toBool();
                
            if (obj.contains("txtEnabled") && obj["txtEnabled"].isBool())
                config.txtEnabled = obj["txtEnabled"].toBool();
                
            if (obj.contains("lastVideoDir") && obj["lastVideoDir"].isString())
                config.lastVideoDir = obj["lastVideoDir"].toString();
        }
    }
}

void MainWindow::saveConfig()
{
    QJsonObject obj;
    obj["srtEnabled"] = ui->srtCheckBox->isChecked();
    obj["txtEnabled"] = ui->txtCheckBox->isChecked();
    
    // 保存最后选择的视频目录
    if (!videoFilePath.isEmpty()) {
        obj["lastVideoDir"] = QFileInfo(videoFilePath).absolutePath();
    } else if (!config.lastVideoDir.isEmpty()) {
        obj["lastVideoDir"] = config.lastVideoDir;
    }
    
    QJsonDocument doc(obj);
    QFile file(configFilePath);
    
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        file.write(doc.toJson());
        file.close();
    }
}

void MainWindow::on_srtCheckBox_stateChanged(int state)
{
    Q_UNUSED(state);
    saveConfig();
}

void MainWindow::on_txtCheckBox_stateChanged(int state)
{
    Q_UNUSED(state);
    saveConfig();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // 只有不在处理时才接受拖放
    if (!isProcessing && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    // 只有不在处理时才处理拖放
    if (!isProcessing) {
        const QMimeData *mimeData = event->mimeData();
        
        if (mimeData->hasUrls()) {
            QList<QUrl> urlList = mimeData->urls();
            
            if (!urlList.isEmpty()) {
                QString filePath = urlList.first().toLocalFile();
                
                // 检查文件是否为视频文件(简单检查扩展名)
                QStringList videoExtensions = {".mp4", ".avi", ".mkv", ".mov", ".wmv"};
                QString fileExtension = QFileInfo(filePath).suffix().toLower();
                
                if (videoExtensions.contains("." + fileExtension)) {
                    videoFilePath = filePath;
                    ui->videoPathLineEdit->setText(videoFilePath);
                    ui->startButton->setEnabled(true);
                    ui->statusLabel->setText("已选择视频文件");
                    
                    // 保存配置
                    saveConfig();
                } else {
                    QMessageBox::warning(this, "警告", "请拖放视频文件");
                }
            }
        }
    }
}

void MainWindow::on_selectVideoButton_clicked()
{
    // 只有不在处理时才允许选择视频
    if (!isProcessing) {
        // 使用上次选择的目录
        QString dir = config.lastVideoDir;
        if (!QDir(dir).exists()) {
            dir = "";
        }
        
        videoFilePath = QFileDialog::getOpenFileName(
            this,
            "选择视频文件",
            dir,
            "视频文件 (*.mp4 *.avi *.mkv *.mov *.wmv);;所有文件 (*)"
        );
        
        if (!videoFilePath.isEmpty()) {
            ui->videoPathLineEdit->setText(videoFilePath);
            ui->startButton->setEnabled(true);
            ui->statusLabel->setText("已选择视频文件");

            // 更新配置中的lastVideoDir
            config.lastVideoDir = QFileInfo(videoFilePath).absolutePath();
            // 保存配置
            saveConfig();
        }
    }
}

void MainWindow::on_startButton_clicked()
{
    if (videoFilePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择视频文件");
        return;
    }
    
    // 检查是否至少选择了一种输出格式
    if (!ui->srtCheckBox->isChecked() && !ui->txtCheckBox->isChecked()) {
        QMessageBox::warning(this, "警告", "请至少选择一种输出格式");
        return;
    }
    
    // 设置处理状态
    isProcessing = true;
    
    // 禁用UI元素
    setUIEnabled(false);
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    
    ui->statusLabel->setText("正在获取视频信息...");
    ui->progressBar->setValue(0);
    
    // 重置进度变量
    totalDurationMs = 0;
    currentDurationMs = 0;
    
    // 重置字幕序号
    subtitleNumber = 1;
    
    // 生成输出文件名
    QString basePath = QFileInfo(videoFilePath).absolutePath() + "/" + 
                       QFileInfo(videoFilePath).completeBaseName();
    outputSrtFilePath = basePath + ".srt";
    outputTxtFilePath = basePath + ".txt";
    
    // 如果需要，先删除之前的文件
    if (ui->srtCheckBox->isChecked() && QFile::exists(outputSrtFilePath)) {
        QFile::remove(outputSrtFilePath);
    }
    
    if (ui->txtCheckBox->isChecked() && QFile::exists(outputTxtFilePath)) {
        QFile::remove(outputTxtFilePath);
    }
    
    // 先获取视频时长
    QStringList args;
    args << "-i" << videoFilePath;
    getVideoDurationProcess->start(getAppPath() + "ffmpeg-win32-x64.exe", args);
}

void MainWindow::on_stopButton_clicked()
{
    if (isProcessing) {
        forceStop = true;
        // 终止所有运行中的进程
        if (getVideoDurationProcess->state() == QProcess::Running) {
            getVideoDurationProcess->kill();
            getVideoDurationProcess->waitForFinished(1000);
        }
        
        if (ffmpegProcess->state() == QProcess::Running) {
            ffmpegProcess->kill();
            ffmpegProcess->waitForFinished(1000);
        }
        
        if (wav2srtProcess->state() == QProcess::Running) {
            wav2srtProcess->kill();
            wav2srtProcess->waitForFinished(1000);
        }
        forceStop = false;
        // 删除临时文件
        QFile::remove(tempWavFilePath);
        
        // 恢复UI状态
        isProcessing = false;
        setUIEnabled(true);
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        ui->statusLabel->setText("已取消处理");
        ui->progressBar->setValue(0);
        
        QMessageBox::information(this, "提示", "处理已停止");
    }
}

void MainWindow::getVideoDurationReadyReadStandardOutput()
{
    QString output = getVideoDurationProcess->readAllStandardOutput();
    ui->logTextEdit->append(output);
}

void MainWindow::getVideoDurationReadyReadStandardError()
{
    QString error = getVideoDurationProcess->readAllStandardError();
    ui->logTextEdit->append(error);
    
    // 尝试从错误输出中提取视频时长
    QRegularExpression durationRegex("Duration: (\\d+):(\\d+):(\\d+\\.\\d+)");
    QRegularExpressionMatch match = durationRegex.match(error);
    
    if (match.hasMatch()) {
        QString hours = match.captured(1);
        QString minutes = match.captured(2);
        QString seconds = match.captured(3);
        
        // 计算总毫秒数
        totalDurationMs = hours.toInt() * 3600000 + 
                         minutes.toInt() * 60000 + 
                         seconds.toDouble() * 1000;
        
        ui->statusLabel->setText("视频时长: " + 
                                QString("%1:%2:%3")
                                .arg(hours)
                                .arg(minutes)
                                .arg(seconds));
    }
}

void MainWindow::getVideoDurationFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (totalDurationMs <= 0) {
        ui->statusLabel->setText("无法获取视频时长，进度显示可能不准确");
    }
    
    // 继续进行音频提取
    ui->statusLabel->setText("正在提取音频...");
    
    // 生成临时文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    tempWavFilePath = QDir::tempPath() + "/temp_audio_" + timestamp + ".wav";
    
    // 构建FFmpeg命令
    QStringList ffmpegArgs;
    ffmpegArgs << "-i" << videoFilePath;
    ffmpegArgs << "-ar" << "16000";
    ffmpegArgs << "-ac" << "1";
    ffmpegArgs << "-c:a" << "pcm_s16le";
    ffmpegArgs << tempWavFilePath;
    
    // 启动FFmpeg进程（使用绝对路径）
    ffmpegProcess->start(getAppPath() + "ffmpeg-win32-x64.exe", ffmpegArgs);
}

void MainWindow::ffmpegReadyReadStandardOutput()
{
    QString output = ffmpegProcess->readAllStandardOutput();
    ui->logTextEdit->append(output);
}

void MainWindow::ffmpegReadyReadStandardError()
{
    QString error = ffmpegProcess->readAllStandardError();
    ui->logTextEdit->append(error);
    
    // 尝试从FFmpeg输出中提取当前处理时间
    QRegularExpression timeRegex("time=(\\d+):(\\d+):(\\d+\\.\\d+)");
    QRegularExpressionMatch match = timeRegex.match(error);
    
    if (match.hasMatch()) {
        QString hours = match.captured(1);
        QString minutes = match.captured(2);
        QString seconds = match.captured(3);
        
        // 计算当前毫秒数
        currentDurationMs = hours.toInt() * 3600000 + 
                           minutes.toInt() * 60000 + 
                           seconds.toDouble() * 1000;
        
        // 计算进度百分比
        int progress = 0;
        if (totalDurationMs > 0) {
            progress = qMin(100, static_cast<int>((currentDurationMs * 100) / totalDurationMs / 2)); // 音频提取占50%进度
        }
        
        ui->progressBar->setValue(progress);
        ui->statusLabel->setText(QString("正在提取音频: %1/%2").arg(
            QString("%1:%2:%3").arg(hours).arg(minutes).arg(seconds),
            formatDuration(totalDurationMs)
        ));
    }
}

QString MainWindow::formatDuration(qint64 ms)
{
    if (ms <= 0) return "00:00:00";
    
    int totalSeconds = ms / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

void MainWindow::ffmpegFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        ui->statusLabel->setText("音频提取完成，正在识别字幕...");
        ui->progressBar->setValue(50); // 音频提取完成，进度设为50%
        
        // 重置当前处理时长
        currentDurationMs = 0;
        
        // 构建wav2srt命令
        QStringList wav2srtArgs;
        wav2srtArgs << "-f" << tempWavFilePath;
        wav2srtArgs << "-m" << "ggml-base.bin";
        wav2srtArgs << "-l" << "zh";

        //解决输出有些时候是繁体中文的问题
        //  https://blog.csdn.net/abcd51685168/article/details/139904153
        wav2srtArgs << "--prompt" << "以下是普通话的句子，这是一段会议记录。";
        wav2srtArgs << "-osrt";
        
        // 启动wav2srt进程（使用绝对路径）
        wav2srtProcess->start(getAppPath() + "wav2srt.exe", wav2srtArgs);
    } else if (forceStop == false) {
        // 恢复UI状态
        isProcessing = false;
        setUIEnabled(true);
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        
        ui->statusLabel->setText("音频提取失败");
        QMessageBox::critical(this, "错误", "FFmpeg处理失败，请检查日志");
    }
}

void MainWindow::wav2srtReadyReadStandardOutput()
{
    QString output = wav2srtProcess->readAllStandardOutput();
    ui->logTextEdit->append(output);
    
    // 正则表达式匹配时间戳格式 [HH:MM:SS.XXX --> HH:MM:SS.XXX]
    QRegularExpression timeStampRegex("\\[(\\d\\d):(\\d\\d):(\\d\\d)\\.(\\d\\d\\d) --> (\\d\\d):(\\d\\d):(\\d\\d)\\.(\\d\\d\\d)\\]");
    
    // 如果需要SRT文件，转换为标准格式
    if (ui->srtCheckBox->isChecked()) {
        QFile srtFile(outputSrtFilePath);
        if (srtFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&srtFile);
            
            // 解析wav2srt输出，转换为标准SRT格式
            QStringList lines = output.split('\n');
            QString currentSubtitleText;
            bool isInSubtitle = false;
            
            for (const QString &line : lines) {
                QString trimmedLine = line.trimmed();
                
                // 跳过空行
                if (trimmedLine.isEmpty()) {
                    continue;
                }
                
                // 检查是否为时间戳行
                QRegularExpressionMatch match = timeStampRegex.match(trimmedLine);
                if (match.hasMatch()) {
                    // 如果有当前字幕，先输出
                    if (isInSubtitle) {
                        out << subtitleNumber << "\n";
                        out << currentSubtitleText << "\n\n";
                        subtitleNumber++;
                    }
                    
                    // 开始新的字幕
                    isInSubtitle = true;
                    
                    // 提取时间戳并转换为标准格式
                    QString startTime = match.captured(1) + ":" + match.captured(2) + ":" + match.captured(3) + "," + match.captured(4);
                    QString endTime = match.captured(5) + ":" + match.captured(6) + ":" + match.captured(7) + "," + match.captured(8);
                    
                    // 提取时间戳后的文本
                    int textStart = trimmedLine.indexOf("]") + 1;
                    QString subtitleText = trimmedLine.mid(textStart).trimmed();
                    
                    // 构建标准SRT时间行
                    currentSubtitleText = startTime + " --> " + endTime + "\n";
                    
                    // 添加字幕文本（如果有）
                    if (!subtitleText.isEmpty()) {
                        currentSubtitleText += subtitleText + "\n";
                    }
                    
                    continue;
                }
                
                // 处理可能的多行字幕文本
                if (isInSubtitle && !trimmedLine.isEmpty()) {
                    currentSubtitleText += trimmedLine + "\n";
                }
            }
            
            // 输出最后一个字幕
            if (isInSubtitle) {
                out << subtitleNumber << "\n";
                out << currentSubtitleText << "\n";
                subtitleNumber++;
            }
            
            srtFile.close();
        }
    }
    
    // 如果需要TXT文件，直接从输出中提取文本
    if (ui->txtCheckBox->isChecked()) {
        QFile txtFile(outputTxtFilePath);
        if (txtFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&txtFile);
            
            // 解析wav2srt输出，提取纯文本
            QStringList lines = output.split('\n');
            bool isFirstLine = true;
            
            for (const QString &line : lines) {
                QString trimmedLine = line.trimmed();
                
                // 跳过空行
                if (trimmedLine.isEmpty()) {
                    continue;
                }
                
                // 检查是否为时间戳行
                if (timeStampRegex.match(trimmedLine).hasMatch()) {
                    // 提取时间戳后的文本
                    int textStart = trimmedLine.indexOf("]") + 1;
                    QString subtitleText = trimmedLine.mid(textStart).trimmed();
                    
                    if (!subtitleText.isEmpty()) {
                        if (!isFirstLine) {
                            out << "\n"; // 字幕块之间添加空行分隔
                        }
                        out << subtitleText << "\n";
                        isFirstLine = false;
                    }
                    
                    continue;
                }
                
                // 处理可能的多行字幕文本
                if (!isFirstLine && !trimmedLine.isEmpty()) {
                    out << trimmedLine << "\n";
                }
            }
            
            txtFile.close();
        }
    }
    
    // 尝试从wav2srt输出中提取当前处理时间
    QRegularExpression timeRegex("\\[(\\d+):(\\d+):(\\d+\\.\\d+) -->");
    QRegularExpressionMatch match = timeRegex.match(output);
    
    if (match.hasMatch()) {
        QString hours = match.captured(1);
        QString minutes = match.captured(2);
        QString seconds = match.captured(3);
        
        // 计算当前毫秒数
        currentDurationMs = hours.toInt() * 3600000 + 
                           minutes.toInt() * 60000 + 
                           seconds.toDouble() * 1000;
        
        // 计算进度百分比(音频提取占50%，字幕识别占50%)
        int progress = 50;
        if (totalDurationMs > 0) {
            progress += qMin(50, static_cast<int>((currentDurationMs * 50) / totalDurationMs));
        }
        
        ui->progressBar->setValue(progress);
        ui->statusLabel->setText(QString("正在识别字幕: %1/%2").arg(
            QString("%1:%2:%3").arg(hours).arg(minutes).arg(seconds),
            formatDuration(totalDurationMs)
        ));
    }
}

void MainWindow::wav2srtReadyReadStandardError()
{
    QString error = wav2srtProcess->readAllStandardError();
    ui->logTextEdit->append(error);
}

void MainWindow::wav2srtFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 删除临时WAV文件
    QFile::remove(tempWavFilePath);
    
    // 恢复UI状态
    isProcessing = false;
    setUIEnabled(true);
    ui->startButton->setEnabled(true);
    ui->stopButton->setEnabled(false);
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        ui->statusLabel->setText("字幕提取完成");
        ui->progressBar->setValue(100);
        
        // 显示成功消息
        QString successMsg = "字幕提取完成";
        bool hasSrt = ui->srtCheckBox->isChecked();
        bool hasTxt = ui->txtCheckBox->isChecked();
        
        // 检查文件是否实际生成
        if (hasSrt && !QFile::exists(outputSrtFilePath)) {
            successMsg += "\n警告: SRT文件未生成";
            hasSrt = false;
        }
        
        if (hasTxt && !QFile::exists(outputTxtFilePath)) {
            successMsg += "\n警告: TXT文件未生成";
            hasTxt = false;
        }
        
        if (hasSrt) {
            successMsg += "\nSRT文件已保存到: " + outputSrtFilePath;
        }
        
        if (hasTxt) {
            successMsg += "\nTXT文件已保存到: " + outputTxtFilePath;
        }
        
        QMessageBox::information(this, "成功", successMsg);
    } else if (forceStop == false) {
        ui->statusLabel->setText("字幕提取失败");
        QMessageBox::critical(this, "错误", "wav2srt处理失败，请检查日志");
    }
}

QString MainWindow::getAppPath() const
{
    return QFileInfo(QCoreApplication::applicationFilePath()).absolutePath() + "/";
}

void MainWindow::setUIEnabled(bool enabled)
{
    ui->selectVideoButton->setEnabled(enabled);
    ui->videoPathLineEdit->setEnabled(enabled);
    ui->srtCheckBox->setEnabled(enabled);
    ui->txtCheckBox->setEnabled(enabled);
}    
