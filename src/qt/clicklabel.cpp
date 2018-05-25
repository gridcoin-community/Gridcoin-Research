#include "clicklabel.h"
#include <QMouseEvent>

ClickLabel::ClickLabel(QWidget *parent, Qt::WindowFlags f)
: QLabel(parent){

}

ClickLabel::~ClickLabel() {}

void ClickLabel::mouseReleaseEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton)
    {
        emit clicked();
    }
}
