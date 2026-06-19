#include "playlistwind.h"
#include "ui_playlistwind.h"

PlayListWind::PlayListWind(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PlayListWind)
{
    ui->setupUi(this);

}

PlayListWind::~PlayListWind()
{
    delete ui;
}

void PlayListWind::on_list_itemClicked(QListWidgetItem *item)
{
    QString selectedText = item->text();
    emit playItemChanged(selectedText); // 发送选中项文本
}
