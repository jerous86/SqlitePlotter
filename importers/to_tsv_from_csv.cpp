#include "to_tsv.h"

void processFile(const QString &filePath) {
	QFile db_in(filePath);
	db_in.open(QIODevice::ReadOnly);
	QTextStream in(&db_in);

	while (!in.atEnd()) {
		std::cout<<in.readLine().replace(",","\t").toStdString()<<"\n";
	}
}

