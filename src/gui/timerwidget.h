#ifndef TIMERWIDGET_H
#define TIMERWIDGET_H

#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QGridLayout>
#include "src/preferences.h"

/**
 * @brief Editable timer for presentation and target time.
 *
 * 2 QLineEdit's "passed" and "total" show presentation time passed and
 * estimate or target for total time.
 *
 * Emits timeout event.
 */
class TimerWidget : public QWidget
{
    Q_OBJECT

    QLineEdit *passed;
    QLineEdit *total;
    QLabel *label;
    QTimer *timer;
    bool timeout = false;

    void updateTimeout() noexcept;

public:
    explicit TimerWidget(QWidget *parent = NULL);
    ~TimerWidget();

    bool hasHeightForWidth() const noexcept override
    {return true;}

    QSize sizeHint() const noexcept override
    {return {150, 20};}

protected:
    /// Resize event: adjust font size.
    void resizeEvent(QResizeEvent *event) noexcept override;

private slots:
    void changePassed();
    void changeTotal();

public slots:
    void updateText() noexcept;
    void updateFullText() noexcept;
    void handleAction(const Action action) noexcept;
    void startTimer() noexcept;
    void stopTimer() noexcept;

signals:
    void sendTimeout(const bool timeout);
};

#endif // TIMERWIDGET_H
