#include <iostream>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

void processFile(const QString &filePath);

int main(int argc, char **args) {
	Q_ASSERT(argc == 2);
	const QString filePath(args[1]);
	Q_ASSERT(QFileInfo(filePath).exists());

	processFile(filePath);
}

void printRow(const QStringList &h1, const QStringList &h2) {
	std::cout<<h1.join("\t").toStdString()<<"\t"<<h2.join("\t").toStdString()<<"\n";
}

void printRow(const QStringList &data, int n_columns) {
	Q_ASSERT(data.size()==n_columns);
	std::cout<<data.join("\t").toStdString()<<"\n";
}
