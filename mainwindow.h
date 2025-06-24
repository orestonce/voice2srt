#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonDocument>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void on_selectVideoButton_clicked();
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void ffmpegReadyReadStandardOutput();
    void ffmpegReadyReadStandardError();
    void ffmpegFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void wav2srtReadyReadStandardOutput();
    void wav2srtReadyReadStandardError();
    void wav2srtFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void getVideoDurationReadyReadStandardOutput();
    void getVideoDurationReadyReadStandardError();
    void getVideoDurationFinished(int exitCode, QProcess::ExitStatus exitStatus);
    
    // 配置改变时保存配置
    void on_srtCheckBox_stateChanged(int state);
    void on_txtCheckBox_stateChanged(int state);

private:
    Ui::MainWindow *ui;
    QProcess *ffmpegProcess;
    QProcess *wav2srtProcess;
    QProcess *getVideoDurationProcess;
    QString videoFilePath;
    QString tempWavFilePath;
    QString outputSrtFilePath;
    QString outputTxtFilePath;
    qint64 totalDurationMs; // 视频总时长(毫秒)
    qint64 currentDurationMs; // 当前处理时长(毫秒)
    bool isProcessing; // 标记是否正在处理
    bool forceStop; // 正在停止
    
    // 配置文件路径
    QString configFilePath;
    
    // 配置选项
    struct Config {
        bool srtEnabled;
        bool txtEnabled;
        QString lastVideoDir;
    } config;
    
    // 加载和保存配置
    void loadConfig();
    void saveConfig();
    
    // 格式化时长的函数声明
    QString formatDuration(qint64 ms);
    
    // 获取应用程序路径
    QString getAppPath() const;
    
    // 启用/禁用UI元素
    void setUIEnabled(bool enabled);
};
#endif // MAINWINDOW_H    