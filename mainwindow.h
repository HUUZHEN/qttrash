#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QComboBox>       // 新增：下拉選單
#include <QLabel>
#include <QTimer>
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

// Qt Charts
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>



    class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateUsage();
    void onCloseButtonClicked();        // 關閉按鈕槽函式
    void onSelectionChanged(const QString &selection); // 工具列表選擇槽函式

private:
    // PDH 相關
    HQUERY m_query;
    HCOUNTER m_cpuCounter;

    QTimer m_updateTimer;
    QLabel *m_cpuLabel;
    QLabel *m_memoryLabel;

    // 關閉監測按鈕
    QPushButton *m_closeButton;

    // 工具列表
    QComboBox *m_toolList;

    // CPU 圖表相關
    QLineSeries *m_cpuSeries;
    QChart *m_cpuChart;
    QChartView *m_cpuChartView;
    qreal m_timeX; // 用來累積時間軸

    // 記憶體圖表相關
    QLineSeries *m_memSeries;
    QChart *m_memChart;
    QChartView *m_memChartView;

    // 監測選項
    bool m_monitorCPU;
    bool m_monitorMemory;

    // 取得目前的記憶體使用量 (以百分比為例)
    double getMemoryUsagePercent();
};

#endif // MAINWINDOW_H
