#ifndef TITLE_H
#define TITLE_H

#include <QWidget>

namespace Ui {
class Title;
}

class Title : public QWidget
{
    Q_OBJECT

public:
    explicit Title(QWidget *parent = 0);
    ~Title();

private slots:
    void on_minBtn_2_clicked();
    void on_maxBtn_clicked();
    void on_fullScreenBtn_clicked();
    void on_closeBtn_clicked();

signals:
    void Decrese_volume();
    void Add_volume();
    void go_ahead();
    void go_back();
private:
    Ui::Title *ui;
};

#endif // TITLE_H
