#include "clicklabel.h"
#include <QMouseEvent>

ClickLabel::ClickLabel(QWidget *parent, Qt::WindowFlags f)
: QLabel(parent){
        this->setMouseTracking(true);
        this->setCursor(Qt::PointingHandCursor);
}

ClickLabel::~ClickLabel() {}

void ClickLabel::mouseReleaseEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton)
    {
        emit clicked();
    }
}
