#include "ctrlbar.h"
#include "ui_ctrlbar.h"
#include "QDebug"
double slider_volume = 0;
double slider_play = 0;
CtrlBar::CtrlBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CtrlBar)
{

    ui->setupUi(this);

    //设置播放图标
    QIcon icon_play(":/ctrl/icon/111.jpg");
    ui->playOrPauseBtn->setIcon(icon_play);

    //设置停止图标
    QIcon icon_stop(":/ctrl/icon/2.jpg");
    ui->stopBtn->setIcon(icon_stop);

    //设置暂停图标
    QIcon icon_pause(":/ctrl/icon/amtls1.jpg");
    ui->forwardBtn->setIcon(icon_pause);
    ui->forwardBtn->setIconSize(ui->forwardBtn->size()); // 图标跟随按钮大小
//    ui->forwardBtn->setIconSize(QSize(30, 30));
//    ui->forwardBtn->setIconSize(QSize(32, 32)); // 按需修改为你需要的尺寸，如(48,48)、(24,24)

    //设置步进step图标
    QIcon icon_step(":/ctrl/icon/zzbf.png");
    ui->backwarkBtn->setIcon(icon_step);
}

CtrlBar::~CtrlBar()
{
    delete ui;
}

void CtrlBar::on_playOrPauseBtn_clicked()
{
    qDebug()<<"Play!!!";
    emit SigPlayOrPause();
}

void CtrlBar::on_stopBtn_clicked()
{
    qDebug()<<"Stop!!!";
    emit Stop();
}

void CtrlBar::on_forwardBtn_clicked()//这里是pause作用
{
    qDebug()<<"Pause!!!";
    emit Pause();
}

void CtrlBar::on_backwarkBtn_clicked()
{
    qDebug()<<"Step!!!";
    emit Step();
}

void CtrlBar::on_volumeSlider_valueChanged(int value)
{

    slider_volume = (double)value;
    emit Change_volume();

}

void CtrlBar::on_volumeBtn_clicked()
{
    qDebug()<<"Muted!!!";
    emit Screenshot_();
}

void CtrlBar::on_playTimeEdit_userTimeChanged(const QTime &time)
{

}

void CtrlBar::on_playSlider_valueChanged(int value)
{
    slider_play = (double)value;
    emit Change_play();
}

void CtrlBar::on_speedBtn_clicked()
{
    qDebug()<<"Speed!!!";
    emit Change_speed();
}
