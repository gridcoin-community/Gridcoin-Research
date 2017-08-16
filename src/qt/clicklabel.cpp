#include "clicklabel.h"

ClickLabel::ClickLabel(QWidget* parent, Qt::WindowFlags f)
: QLabel(parent){

}

ClickLabel::~ClickLabel() {}

void ClickLabel::mousePressEvent(QMouseEvent* event) {
    emit clicked();
}
