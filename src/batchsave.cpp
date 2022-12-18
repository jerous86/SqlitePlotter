#include "batchsave.h"
#include "ui_batchsave.h"

#include <QDir>

#include "plot.h"
#include "highlighters.h"
#include "misc.h"

BatchSave::BatchSave(QWidget *parent, const QList<Plot *> &plots_, const QString &default_,
	const QString &db_root_) : QDialog(parent), ui(new Ui::BatchSave), db_root(db_root_), plots(plots_) {
	ui->setupUi(this);
	hl=new MyBatchSaveHighlighter(ui->txtInput);
	ui->txtInput->setText(default_);
	accept_state=0;

	connect(this, &QDialog::accepted, [this]() { accept_state=1; });
	connect(this, &QDialog::rejected, [this]() { accept_state=-1; });
}

BatchSave::~BatchSave() {
	delete hl;
	delete ui;
}

QString BatchSave::getText() const { return ui->txtInput->toPlainText();  }


static QRegularExpression rgxComment=QRegularExpression("#.*$");
QStringList BatchSave::lines() const {
	const QString data=ui->txtInput->toPlainText().trimmed();

	QStringList lines=data.split("\n");
	for(int i=lines.size()-1; i>=0; i--) {
		lines[i]=lines[i].replace(rgxComment, "");
		if (lines[i].trimmed().length()==0 || lines[i].trimmed().startsWith('#')) {
			lines.removeAt(i);
		}
	}
	return lines;
}

QString BatchSave::pattern(const QStringList &lines) const {
	if (lines.size()>0) {
		QString ret=QString(lines[0]).replace("~", QDir::homePath());
		if (!ret.startsWith("/") && !ret.startsWith("$")) { ret=QDir::currentPath()+"/"+ret; }
		return ret;
	} else {
		return "";
	}
}

QMap<QString, QStringList> BatchSave::cp_vars(const QStringList &lines) const {
	return lines_to_map(lines);
}


unsigned BatchSave::n_combinations(const QMap<QString, QStringList> &cp_vars) const {
	return ::n_combinations(cp_vars);
}

QMap<QString, QString> BatchSave::nth_combination(const QMap<QString, QStringList> &cp_vars, unsigned i) const {
	MapQStringQString override_vars=::nth_combination(cp_vars, i);
	override_vars["PLOT_INDEX"]=QString::number(i);
	override_vars["DB_ROOT"]=db_root;
	return override_vars;
}

void BatchSave::on_txtInput_textChanged() {
	const QStringList lines=this->lines();
	const QString pattern=this->pattern(lines);
	const QMap<QString,QStringList> cp_vars=this->cp_vars(lines);
	const unsigned n_combinations=this->n_combinations(cp_vars);

	ui->lstPreview->clear();
	ui->lstPreview->addItems(QStringList()
		<<"PATTERN "+pattern
        <<QString("%1 images to generate (%2 plots * %3 combinations)").arg(n_combinations*unsigned(plots.size())).arg(plots.size()).arg(n_combinations));
	for(Plot *plot : plots) {
		for(unsigned i=0; i<n_combinations; i++) {
			const QString tgt=plot->apply_variables(pattern, nth_combination(cp_vars, i));
			ui->lstPreview->addItem(tgt);
		}
	}
}
