#ifndef PLAYLISTWIND_H
#define PLAYLISTWIND_H

#include <QWidget>
#include <QListWidgetItem> // 新增头文件
namespace Ui {
class PlayListWind;
}

class PlayListWind : public QWidget
{
    Q_OBJECT

public:
    explicit PlayListWind(QWidget *parent = 0);
    ~PlayListWind();
signals:
     void playItemChanged(const QString &itemText); // 选中项变化时发送的信号

private slots:
    void on_list_itemClicked(QListWidgetItem *item);

private:
    Ui::PlayListWind *ui;

};

#endif // PLAYLISTWIND_H
