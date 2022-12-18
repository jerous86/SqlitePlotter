#pragma once

// https://stackoverflow.com/a/39435345

#include <QHBoxLayout>

class QHeaderView;

class ColumnAlignedLayout : public QHBoxLayout {
	Q_OBJECT
public:
	ColumnAlignedLayout();
	explicit ColumnAlignedLayout(QWidget *parent);
	void setTableColumnsToTrack(QHeaderView *view) { headerView = view; }

	QSize minimumSize() const override;
private:
	void setGeometry(const QRect &r) override;
	QHeaderView *headerView;
};
