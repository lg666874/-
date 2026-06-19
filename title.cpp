#include "title.h"
#include "ui_title.h"
#include <QDebug>
Title::Title(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Title)
{
    ui->setupUi(this);

    QIcon icon_menu(":/ctrl/icon/tx.png");
    ui->menuBtn->setIcon(icon_menu);
    ui->menuBtn->setIconSize(QSize(120, 120));
}

Title::~Title()
{
    delete ui;
}

void Title::on_minBtn_2_clicked()
{
    qDebug()<<"Drease volume!!!";
    emit Decrese_volume();
}

void Title::on_maxBtn_clicked()
{
    qDebug()<<"Add volume!!!";
    emit Add_volume();
}

void Title::on_fullScreenBtn_clicked()
{
    qDebug()<<"go_Ahead !!!";
    emit go_ahead();
}

void Title::on_closeBtn_clicked()
{
    qDebug()<<"go_back !!!";
    emit go_back();
}
