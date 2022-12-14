
#include "columnalignedlayout.h"
#include <QHeaderView>
#include <QDebug>

ColumnAlignedLayout::ColumnAlignedLayout() : QHBoxLayout() {  }

ColumnAlignedLayout::ColumnAlignedLayout(QWidget *parent) : QHBoxLayout(parent) {  }

QSize ColumnAlignedLayout::minimumSize() const { return headerView->minimumSize(); }

void ColumnAlignedLayout::setGeometry(const QRect &r) {
    QHBoxLayout::setGeometry(r);

    Q_ASSERT(parentWidget());
    Q_ASSERT_X(headerView, "layout", "no table columns to track");
    Q_ASSERT_X(headerView->count() == count(), "layout", "there must be as many items in the layout as there are columns in the table");

    if (!headerView) { return; }
    if (headerView->count() != count()) { return; }

    int widgetX = parentWidget()->mapToGlobal(QPoint(0, 0)).x();
    int headerX = headerView->mapToGlobal(QPoint(0, 0)).x();
    int delta = headerX - widgetX;

    for (int ii = 0; ii < headerView->count(); ++ii) {
        int pos = headerView->sectionViewportPosition(ii);
        int size = headerView->sectionSize(ii);

        auto item = itemAt(ii);
        auto r = item->geometry();
        r.setLeft(pos + delta);
        r.setWidth(size);
        item->setGeometry(r);
    }
}
