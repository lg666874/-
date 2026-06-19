#ifndef CTRLBAR_H
#define CTRLBAR_H

#include <QWidget>

namespace Ui {
class CtrlBar;
}

class CtrlBar : public QWidget
{
    Q_OBJECT

public:
    explicit CtrlBar(QWidget *parent = 0);
    ~CtrlBar();
signals:
    void SigPlayOrPause();
    void Stop();
    void Pause();
    void Step();
    void Change_volume();
    void Change_play();
    void Change_speed();
    void Screenshot_();

private slots:
    void on_playOrPauseBtn_clicked();

    void on_stopBtn_clicked();

    void on_forwardBtn_clicked();

    void on_backwarkBtn_clicked();

    void on_volumeSlider_valueChanged(int value);

    void on_volumeBtn_clicked();

    void on_playTimeEdit_userTimeChanged(const QTime &time);

    void on_playSlider_valueChanged(int value);

    void on_speedBtn_clicked();

private:
    Ui::CtrlBar *ui;
};

#endif // CTRLBAR_H
