#pragma once

#include <QColor>
#include <QTextStream>
#include <QRegularExpression>
extern QString lastMsg;
void log(const QString &str, const QColor &color);
QVariant str_to_qvariant(const QString &txt);

#define POS __FILE__<<":"<<__LINE__
#define LOG_(STREAM, COLOR) { \
		QString line; \
		QTextStream stream(&line); \
		stream<<POS<<" "<<STREAM; \
		log(stream.readAll(), COLOR); \
	}

#define LOG(STREAM)	LOG_(STREAM, Qt::black)
#define LOG_INFO(STREAM)	{ qDebug()<<POS<<" "<<STREAM;       LOG_(STREAM, QColor(Qt::black)); } (void)0
#define LOG_ERROR(STREAM)	{ qWarning()<<POS<<" [E] "<<STREAM; LOG_("[E] "<<STREAM, QColor(Qt::red)); } (void)0
#define LOG_WARNING(STREAM)	{ qWarning()<<POS<<" [W] "<<STREAM; LOG_("[W] "<<STREAM, QColor(Qt::magenta)); } (void)0

#define DEBUG(STR)	qDebug()<<POS<<" "<<STR

#define BLOCKED_CALL(OBJ, CALL)  OBJ->blockSignals(true); OBJ->CALL; OBJ->blockSignals(false);

QMap<QString, QStringList> lines_to_map(const QStringList &lines, const QRegularExpression &splitter=QRegularExpression("\\s"), unsigned lines_offset=0);
unsigned n_combinations(const QMap<QString, QStringList> &cp_vars);
QMap<QString, QString> nth_combination(const QMap<QString, QStringList> &cp_vars, unsigned i);


QStringList run(const QString &prog, const QStringList &args, int wait_ms=30*1000);
QString pick_color(int i);
