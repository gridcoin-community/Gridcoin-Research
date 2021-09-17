#ifndef BITCOIN_QT_CLICKLABEL_H
#define BITCOIN_QT_CLICKLABEL_H

#include <QLabel>
#include <QWidget>
#include <Qt>

class ClickLabel: public QLabel
{
    Q_OBJECT
public:
    explicit ClickLabel(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~ClickLabel();
signals:
    void clicked();
protected:
    void mouseReleaseEvent(QMouseEvent *event);
};

#endif // BITCOIN_QT_CLICKLABEL_H
