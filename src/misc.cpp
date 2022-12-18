#include "misc.h"

#include <QMap>
#include <QString>

#include "plot.h" // for MapQStringQString

QString pick_color(int i) {
    // http://colorbrewer2.org/#type=qualitative&scheme=Paired&n=12
    QList<QString> colors=QList<QString>()<</*"#a6cee3" <<*/"#1f78b4" <<"#b2df8a" <<"#33a02c" <<"#fb9a99" <<"#e31a1c"
        <<"#fdbf6f" <<"#ff7f00" <<"#cab2d6" <<"#6a3d9a" <<"#aaaa66" <<"#b15928";
    return colors[i % colors.size()];
}


QMap<QString, QStringList> lines_to_map(const QStringList &lines, const QRegularExpression &splitter, unsigned lines_offset) {
	QMap<QString,QStringList> cp_vars; // cross-product variables

//	unsigned n_combinations=1;
	for(int i=int(lines_offset); i<lines.size(); i++) {
        if (!lines[i].trimmed().startsWith("#") && lines[i].contains("=")) {
            const int j=lines[i].indexOf("=");
            const QString var=lines[i].mid(0,j-1).trimmed();
            const QStringList xs=lines[i].mid(j+1).trimmed().split(splitter);

			// Only allow sensible variable names. This is useful in case we e.g. forget to remove the filename,
			// which probably contains dots, slashes etc
            if (!var.contains(QRegularExpression("^[a-zA-Z_0-9-]+$"))) {
                LOG_INFO("Not using variable "<<var<<" -- it contains non-alphanumeric characters");
				continue;
			}

			if (xs.size()>0) {
                cp_vars[var.trimmed()]=xs;
//				n_combinations*=unsigned(xs.size());
			}
		}
	}

	return cp_vars;
}

unsigned n_combinations(const QMap<QString, QStringList> &cp_vars) {
	unsigned n_combinations=1;
    for(auto &var:cp_vars) { n_combinations *= unsigned(var.size()); }
	return n_combinations;
}

QMap<QString, QString> nth_combination(const QMap<QString, QStringList> &cp_vars, unsigned i) {
	MapQStringQString override_vars;
    int j=int(i);
	for(QString x:cp_vars.keys()) {
        override_vars[x]=cp_vars[x][j%int(cp_vars[x].size())];
        j=(j-j%int(cp_vars[x].size()))/int(cp_vars[x].size());
	}
	return override_vars;
}
