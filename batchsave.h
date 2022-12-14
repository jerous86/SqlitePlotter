#pragma once

#include <QDialog>

struct Plot;
class MyBatchSaveHighlighter;

namespace Ui {
class BatchSave;
}

class BatchSave : public QDialog {
	Q_OBJECT

public:
	explicit BatchSave(QWidget *parent, const QList<Plot *> &plots, const QString &default_, const QString &db_root_);
	~BatchSave();

	QString getText() const;

	MyBatchSaveHighlighter *hl;
	Ui::BatchSave *ui;

	const QString db_root;

	const QList<Plot *> &plots;
	int accept_state=0; // 0: not know, 1: yes, -1: no

	QStringList lines() const;
	QString pattern(const QStringList &lines) const;
	QMap<QString, QStringList> cp_vars(const QStringList &lines) const;
	unsigned n_combinations(const QMap<QString, QStringList> &cp_vars) const;
	QMap<QString,QString> nth_combination(const QMap<QString, QStringList> &cp_vars, unsigned i) const;
private slots:
	void on_txtInput_textChanged();
};
