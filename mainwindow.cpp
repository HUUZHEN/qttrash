#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <psapi.h>  // 用於處理記憶體資訊 (Windows)
#include <pdh.h>    // 用於處理性能計數器 (Windows)
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_query(nullptr),
    m_cpuCounter(nullptr),
    m_cpuLabel(new QLabel(this)),
    m_memoryLabel(new QLabel(this)),
    m_closeButton(new QPushButton("關閉視窗", this)),
    m_toolList(new QComboBox(this)),
    m_cpuSeries(new QLineSeries()),
    m_cpuChart(new QChart()),
    m_cpuChartView(new QChartView(m_cpuChart)),
    m_memSeries(new QLineSeries()),
    m_memChart(new QChart()),
    m_memChartView(new QChartView(m_memChart)),
    m_timeX(0.0),
    m_monitorCPU(true),
    m_monitorMemory(true)
{
    // 視窗大小
    resize(800, 600);

    // 建立中央容器
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    // 主版面配置
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // 1. 工具列表與狀態顯示
    QHBoxLayout *topLayout = new QHBoxLayout();

    // 設定工具列表選項
    m_toolList->addItem("監測 CPU");
    m_toolList->addItem("監測 記憶體");
    m_toolList->addItem("監測 CPU & 記憶體");
    m_toolList->setCurrentIndex(2); // 預設選擇同時監測 CPU 和記憶體

    // 連接選擇變化訊號至槽函式
    connect(m_toolList, &QComboBox::currentTextChanged, this, &MainWindow::onSelectionChanged);

    // 加入工具列表至左上角
    topLayout->addWidget(m_toolList);
    topLayout->addStretch(); // 彈性空間，將工具列表推至左側

    // 加入狀態顯示 Label
    m_cpuLabel->setText("CPU Usage: -- %");
    m_memoryLabel->setText("Memory Usage: -- %");
    topLayout->addWidget(m_cpuLabel);
    topLayout->addWidget(m_memoryLabel);

    mainLayout->addLayout(topLayout);

    // 2. 設定 CPU 圖表
    m_cpuSeries->setName("CPU %");
    m_cpuChart->addSeries(m_cpuSeries);
    m_cpuChart->legend()->hide();
    m_cpuChart->setTitle("CPU Usage");
    QValueAxis *cpuAxisX = new QValueAxis;
    cpuAxisX->setRange(0, 60);
    cpuAxisX->setLabelFormat("%g");
    cpuAxisX->setTitleText("Time (s)");
    QValueAxis *cpuAxisY = new QValueAxis;
    cpuAxisY->setRange(0, 100);
    cpuAxisY->setTitleText("CPU % usage");
    m_cpuChart->addAxis(cpuAxisX, Qt::AlignBottom);
    m_cpuChart->addAxis(cpuAxisY, Qt::AlignLeft);
    m_cpuSeries->attachAxis(cpuAxisX);
    m_cpuSeries->attachAxis(cpuAxisY);
    m_cpuChartView->setRenderHint(QPainter::Antialiasing);

    // 3. 設定記憶體圖表
    m_memSeries->setName("Memory %");
    m_memChart->addSeries(m_memSeries);
    m_memChart->legend()->hide();
    m_memChart->setTitle("Memory Usage");
    QValueAxis *memAxisX = new QValueAxis;
    memAxisX->setRange(0, 60);
    memAxisX->setLabelFormat("%g");
    memAxisX->setTitleText("Time (s)");
    QValueAxis *memAxisY = new QValueAxis;
    memAxisY->setRange(0, 100);
    memAxisY->setTitleText("Memory % usage");
    m_memChart->addAxis(memAxisX, Qt::AlignBottom);
    m_memChart->addAxis(memAxisY, Qt::AlignLeft);
    m_memSeries->attachAxis(memAxisX);
    m_memSeries->attachAxis(memAxisY);
    m_memChartView->setRenderHint(QPainter::Antialiasing);

    // 4. 加入圖表至版面
    QHBoxLayout *chartLayout = new QHBoxLayout();
    chartLayout->addWidget(m_cpuChartView);
    chartLayout->addWidget(m_memChartView);
    mainLayout->addLayout(chartLayout);

    // 5. 加入關閉監測按鈕
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(); // 彈性空間，將按鈕推至右側
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    // 6. 初始化 PDH
    PDH_STATUS status = PdhOpenQuery(NULL, 0, &m_query);
    if (status == ERROR_SUCCESS)
    {
        status = PdhAddCounter(m_query, L"\\Processor(_Total)\\% Processor Time", 0, &m_cpuCounter);
        if (status != ERROR_SUCCESS)
            qDebug() << "無法添加 PDH 計數器。";
    }
    else {
        qDebug() << "無法開啟 PDH 查詢。";
    }

    // 7. 設置更新計時器 (每秒更新一次)
    connect(&m_updateTimer, &QTimer::timeout, this, &MainWindow::updateUsage);
    m_updateTimer.start(1000);

    // 8. 連接關閉按鈕與槽函式
    connect(m_closeButton, &QPushButton::clicked, this, &MainWindow::onCloseButtonClicked);
}

MainWindow::~MainWindow()
{
    if (m_query) {
        PdhCloseQuery(m_query);
    }
}

// 取得記憶體使用率（百分比）
double MainWindow::getMemoryUsagePercent()
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        ULONGLONG used = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        return (static_cast<double>(used) / static_cast<double>(memInfo.ullTotalPhys)) * 100.0;
    }
    return 0.0;
}

// 更新 CPU / 記憶體使用率 & 圖表
void MainWindow::updateUsage()
{
    // CPU
    double cpuVal = 0.0;
    if (m_query && m_cpuCounter && m_monitorCPU)
    {
        PDH_STATUS status = PdhCollectQueryData(m_query);
        if (status == ERROR_SUCCESS)
        {
            PDH_FMT_COUNTERVALUE counterVal;
            ZeroMemory(&counterVal, sizeof(counterVal));

            status = PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, NULL, &counterVal);
            if (status == ERROR_SUCCESS)
            {
                cpuVal = counterVal.doubleValue;
                m_cpuLabel->setText(QString("CPU Usage: %1 %").arg(cpuVal, 0, 'f', 2));

                // 更新 CPU 圖表資料
                m_timeX += 1.0;
                m_cpuSeries->append(m_timeX, cpuVal);
                if (m_timeX > 60)
                {
                    m_cpuChart->axisX()->setRange(m_timeX - 60, m_timeX);
                }
            }
        }
    }

    // 記憶體
    if (m_monitorMemory)
    {
        double memVal = getMemoryUsagePercent();
        m_memoryLabel->setText(QString("Memory Usage: %1 %").arg(memVal, 0, 'f', 2));

        // 更新記憶體圖表資料
        if (m_monitorCPU) { // 如果有 CPU 圖表，使用相同的時間軸
            m_memSeries->append(m_timeX, memVal);
        } else { // 否則獨立累積時間軸
            m_timeX +=1.0;
            m_memSeries->append(m_timeX, memVal);
        }

        if (m_timeX > 60)
        {
            m_memChart->axisX()->setRange(m_timeX - 60, m_timeX);
        }
    }
}

// 關閉按鈕槽函式
void MainWindow::onCloseButtonClicked()
{
    qDebug() << "關閉視窗按鈕被點擊，關閉主視窗。";
    close(); // 關閉主視窗
}

// 工具列表選擇槽函式
void MainWindow::onSelectionChanged(const QString &selection)
{
    if (selection == "監測 CPU")
    {
        m_monitorCPU = true;
        m_monitorMemory = false;

        // 顯示 CPU 圖表
        m_cpuChartView->show();
        // 隱藏記憶體圖表
        m_memChartView->hide();
        m_memoryLabel->setText("Memory Usage: 停止監測");

        // 清除記憶體圖表資料
        m_memSeries->clear();
    }
    else if (selection == "監測 記憶體")
    {
        m_monitorCPU = false;
        m_monitorMemory = true;

        // 顯示記憶體圖表
        m_memChartView->show();
        // 隱藏 CPU 圖表
        m_cpuChartView->hide();
        m_cpuLabel->setText("CPU Usage: 停止監測");

        // 清除 CPU 圖表資料
        m_cpuSeries->clear();
    }
    else if (selection == "監測 CPU & 記憶體")
    {
        m_monitorCPU = true;
        m_monitorMemory = true;

        // 顯示所有圖表
        m_cpuChartView->show();
        m_memChartView->show();

        // 重置標籤
        m_cpuLabel->setText("CPU Usage: -- %");
        m_memoryLabel->setText("Memory Usage: -- %");
    }

    // 根據選擇清除相應的圖表資料
    if (m_monitorCPU)
        m_cpuSeries->clear();
    if (m_monitorMemory)
        m_memSeries->clear();

    // 重置時間軸
    m_timeX = 0.0;
}
