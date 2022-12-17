#include "db.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>

#include <QSet>

#include "misc.h"

extern "C" {
	int RegisterExtensionFunctions(sqlite3 *db); // see libsqlitefunctions.c
	int sqlite3_percentile_init( sqlite3 *db); // see percentile.c
}

#define TABLE_TSV           DEFAULT_TABLE_NAME

QString tableFromQuery(const QString &query) {
    static QRegularExpression rgxTable("SELECT.*?FROM\\s+([A-Za-z_0-9]+).*WHERE",
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch m=rgxTable.match(query);
    return m.hasMatch() ? m.captured(1) : DEFAULT_TABLE_NAME;
}


void Importer::insertRow(const QStringList &data) {
	if (data.size()!=_columnNames.size()) {
		qDebug()<<data.size()<<" vs "<<_columnNames.size();
		qDebug()<<data;
		qDebug()<<_columnNames;
	}
	Q_ASSERT(data.size() == _columnNames.size());
	Db::Stmt stmt=db->query(QString("INSERT INTO %1 (%2) VALUES (%3)")
							.arg(_tableName)
							.arg(_columnNames.join(','))
							.arg(_columnsParams.join(",")));

	for(int c=0; c<data.size(); c++) { stmt.bind(c+1, data[c]); }
	stmt.exec();
}


struct TsvImporter: Importer {
    QString name() const override { return "tsv"; }
    bool canHandleFilePath(const QString &filePath) override { return filePath.endsWith(".tsv"); }
    QStringList patterns() const override { return QStringList()<<"*.tsv"; }

    const QString sep="\t";

    QFile db_in;
    QTextStream in;
    QStringList _columns;
	virtual void openFile(const QString &filePath_) override {
        db_in.setFileName(filePath_);
        db_in.open(QIODevice::ReadOnly);
		in.setDevice(&db_in);

        // We assume we always have a header here
        QString header=in.readLine();
		if (header.startsWith("#")) { header=header.mid(1); }
//        header=header.replace('#', "");

        _columns=header.split(sep);
        for(QString &h:_columns) { h=h.trimmed(); }
    }
	virtual void closeFile() override { db_in.close(); }
	virtual QString tableName() const override { return TABLE_TSV; }
	virtual QStringList columnNames() override { return _columns; }

    void readAllRows() override {
        while (!in.atEnd()) {
            QStringList data=in.readLine().split(sep);
            for(QString &r:data) {
                while (r.startsWith('"') && r.endsWith('"')) {
                    r=r.mid(1,r.length()-2);
                }
            }
            insertRow(data);
        }
    }
};

#include <QProcess>
struct ExtCmdImporter: TsvImporter {
	ExtCmdImporter(const QString &name_, const QString &tableName_,
		const QStringList &patterns_, const QString &command_):
		_name(name_), _tableName(tableName_), _patterns(patterns_), _command(command_) {
		qDebug()<<"\t\t\tNew importer "<<name_;
		qDebug()<<"\t\t\t\tTable: "<<tableName_;
		qDebug()<<"\t\t\t\tPatterns: "<<patterns_;
		qDebug()<<"\t\t\t\tCommand: "<<command_;
	}
	const QString _name;
	const QString _tableName;
	const QStringList _patterns;
	const QString _command;

	QString name() const override { return _name; }
	bool canHandleFilePath(const QString &filePath) override {
		for(const QString &pattern:_patterns) {
			//const auto re=QRegularExpression(QRegularExpression::wildcardToRegularExpression(pattern));
			const auto re=QRegularExpression(QString(pattern).replace(".","${DOT}").replace("*",".*").replace("?",".").replace("${DOT}","\\."));
			if (re.match(filePath).hasMatch()) { return true; }
		}
		return false;
	}
	QStringList patterns() const override { return _patterns; }

    void openFile(const QString &filePath_) override {
		QStringList args=QString(_command)
			.replace("${FILE}", filePath_)
			.replace("${CWD}", QDir::currentPath())
			.replace("${PWD}", QDir::currentPath())
			.replace("${BIN_DIR}", QCoreApplication::applicationDirPath())
			.split(" ");
        QString bin=args[0];
        args.removeFirst();
        const QString tmpFile="/tmp/sqlitePlotter_ExtCmd_input.tsv";

		QProcess proc;
		proc.setStandardOutputFile(tmpFile);
		qDebug()<<"ExtCmdImporter: Running "<<bin<<args.join(" ");
        proc.start(bin, args);
		proc.waitForFinished();

		TsvImporter::openFile(tmpFile);
		Q_ASSERT(columnNames().size()>0);
	}
	QString tableName() const override { return _tableName; }
};


void Db::import(const QString &filePath, Importer *importer) {
    importer->db=this;
    importer->openFile(filePath);
    importer->_tableName=importer->tableName();
    importer->_columnNames=importer->columnNames();
    importer->_columnsParams=QVector<QString>(importer->_columnNames.size(), "?").toList();

    if (query(QString("CREATE TABLE IF NOT EXISTS %1 (%2)")
              .arg(importer->_tableName)
              .arg(importer->_columnNames.join(","))).exec()!=SQLITE_DONE) {
        LOG_ERROR("Failed to create table while loading file "+filePath);
		LOG_ERROR("  for table "+importer->_tableName);
		LOG_ERROR("  and columns "+importer->_columnNames.join(","));
        return;
    }

    importer->readAllRows();
    importer->closeFile();
}

void Db::open_db() {
	Q_ASSERT(!db);
    int filesLoaded=0;
    if (dbFiles.size()==1 && dbFiles[0].endsWith(".sqlite3")) {
        sqlite3_open(dbFiles[0].toStdString().c_str(), &db);
        filesLoaded++;
    } else {
        const int rc=sqlite3_open(":memory:", &db);
        if (rc!=SQLITE_OK) { LOG_ERROR("opening SQLite db in memory "<<sqlite3_errmsg(db)); return; }

        for(const QString &dbFile : dbFiles) {
            bool loaded=false;
            for(Importer *importer : importers) {
                if (importer->canHandleFilePath(dbFile)) {
                    LOG("Reading "<<importer->name()<<" "<<dbFile<<" into "<<importer->tableName());
                    import(dbFile, importer);
                    loaded=true;
                    filesLoaded++;
                    
                    // If we enable the break, then we only use the first matching importer.
                    // By disabling the break, we will use all matching importers.
                    // E.g. for pcaps, we can import just regular stats and ip packets.
                    //break;
                }
            }
            if (!loaded) {
                LOG_WARNING("Not loading file "<<dbFile<<" as no suitable importer was found");
            }
        }
    }

    LOG_INFO("Loaded "<<filesLoaded<<" files");

	Q_ASSERT(db); RegisterExtensionFunctions(db);
	Q_ASSERT(db); sqlite3_percentile_init(db);

	Q_ASSERT(db);
}

void Db::close_db() { sqlite3_close(db); db=nullptr; }

void Db::save_db(const QString &filePath) const {
	sqlite3 *toFile;
	const int rc = sqlite3_open(filePath.toUtf8(), &toFile);
	if (rc!=SQLITE_OK) { LOG_ERROR("Failed to open target file "<<filePath); return; }
	sqlite3_backup *backup=sqlite3_backup_init(toFile, "main", db, "main");
	if (!backup) { LOG_ERROR("call to sqlite3_backup_init failed"); return; }
	sqlite3_backup_step(backup, -1);
	sqlite3_backup_finish(backup);
	sqlite3_close(toFile);
}

Db::Stmt Db::query(const QString &query, int verbosity) {
	return Stmt(*this, query, verbosity);
}

QStringList Db::distinctValues(const QString &column, const QString &table, int verbosity) {
	auto stmt=query(QString("SELECT DISTINCT %1 FROM %2 ORDER BY %1").arg(column).arg(table), verbosity);
	QStringList ret;
	QList<QVariant> row;
	while ((row=stmt.next_row()).size()>0) {
		ret<<row[0].toString();
	}
	return ret;
}

bool Db::table_exists(const QString &table) {
	auto stmt=query(QString("SELECT name FROM sqlite_master WHERE type='table' AND name=='%1'")
		.arg(table));
	auto row=stmt.next_row();
	return row.size()>0;
	
}

unsigned Db::row_count(const QString &table, const QString &where_clause) {
	if (table_exists(table)) {
		auto stmt=query(QString("SELECT COUNT(*) FROM %1 WHERE %2").arg(table).arg(where_clause));
		auto row=stmt.next_row();
		return row.size() ? row[0].toUInt() : 0;
	} else {
		return 0;
	}
}

Db::Stmt::Stmt(Db &db_, const QString &query_, int verbosity): db(db_), query(query_) {
	if (verbosity>=2) { DEBUG("Creating Stmt "<<query); }
	if (query.trimmed().length()==0) { if (verbosity==0) { LOG_ERROR("Empty query"); return; } }

	const int res=sqlite3_prepare_v2(db.db, query.toStdString().c_str(), -1, &stmt, nullptr);
	if (res!=SQLITE_OK) {
		errorMsg=sqlite3_errmsg(db.db);
		LOG_ERROR("sqlite3_prepare_v2 return error "<<res<<" "<<errorMsg<<" "<<" for \n"<<query);
	} else if (!stmt) {
		errorMsg=sqlite3_errmsg(db.db);
		if (verbosity>=1) {
			LOG_ERROR("");
			LOG_ERROR(query);
			LOG_ERROR(errorMsg);
			LOG_ERROR("");
		}
	} else {
		errorMsg=QString();
	}
	db.errorMsg=errorMsg;
}

Db::Stmt::~Stmt() { sqlite3_finalize(stmt); }

int Db::Stmt::column_count() const { return sqlite3_column_count(stmt); }

unsigned Db::Stmt::row_count() {
	unsigned i=0;
	while (next_row().count()>0) { i++; }
	return i;
}

QStringList Db::Stmt::column_names() const {
	QStringList ret;
	const int num_cols = sqlite3_column_count(stmt);
	//	Q_ASSERT_X(num_cols>0, "column_names()", query.toStdString().c_str());
	for (int i = 0; i < num_cols; i++) {
		const QString col_name=QString::fromLocal8Bit(sqlite3_column_name(stmt,i)).toUpper().trimmed();
		ret<<col_name;
	}
	return ret;
}

int Db::Stmt::exec() {
	const int result_code=sqlite3_step(stmt);
	if (result_code!=SQLITE_DONE) {
		LOG_ERROR("sqlite3_step \n\n\t"<<query<<"\n\n returned result code "<<result_code);
		LOG_ERROR(sqlite3_errmsg(db.db));
	}
	return result_code;
}

void Db::Stmt::bind(int i, const QString &value) {
	bool ok;
	{
		const double v=value.toDouble(&ok);
		if (ok) { sqlite3_bind_double(stmt, i, v); return; }
	}

	// If we cannot parse into a double, then assume text.
	sqlite3_bind_text(stmt, i, value.toUtf8(), value.length(), SQLITE_TRANSIENT);
}

QList<QVariant> Db::Stmt::next_row() {
	QList<QVariant> row;
	const int r=sqlite3_step(stmt);
	if (r == SQLITE_DONE) {
	} else if (r==SQLITE_ROW) {
		const int n=column_count();
		for(int col=0; col<n; col++) {
			switch (sqlite3_column_type(stmt, col)) {
			case (SQLITE3_TEXT): {
				const QString txt=QString::fromLocal8Bit(reinterpret_cast<const char *>(sqlite3_column_text(stmt, col)));

				// I don't trust sqlite3's type guessing ...
				row<<str_to_qvariant(txt);
				break;
			}
			case (SQLITE_INTEGER): row<<QVariant(sqlite3_column_int(stmt, col)); break;
			case (SQLITE_FLOAT): row<<QVariant(sqlite3_column_double(stmt, col)); break;
			case (SQLITE_NULL): row<<QVariant("(NULL)"); break;
			default: DEBUG(sqlite3_column_type(stmt, col)); Q_ASSERT(false); break;
			}
		}
	} else if (r==SQLITE_BUSY) { LOG_WARNING("SQLITE_BUSY "<<query);
	} else if (r==SQLITE_ERROR) { LOG_WARNING("SQLITE_ERROR "<<query);
	} else if (r==SQLITE_MISUSE) { LOG_WARNING("SQLITE_MISUSE "<<query);
	} else { LOG_WARNING("next_row returned "<<r<<query);
	}
	return row;
}

QVector<Importer*> importers;
void init_importers() {
	QSet<QString> read_files;
	importers<<new TsvImporter();

	qDebug()<<"Loading importers ...";
	
	const QStringList dirs=QStringList()
		// This path assumes all necessary files are inside a single folder.
		// I think this is the best option, as storing all these files together provides
		// the cleanest and easiest experience.
		<<QCoreApplication::applicationDirPath()
		//<<QCoreApplication::applicationDirPath()+"/../"
		//<<QDir::currentPath()
		;
	for(const QString &dir:dirs) {
		qDebug()<<"Checking dir "<<dir<<" for pattern sqliteplotter-importers*.ini";
		for(const QString &fileName:QDir(dir).entryList(QStringList()<<"sqliteplotter-importers*.ini")) {
			const QString filePath=dir+"/"+fileName;
			if (read_files.contains(filePath)) { continue; }
			read_files.insert(filePath);
			
			QSettings settings(filePath, QSettings::IniFormat);
			qDebug()<<settings.fileName();
			for(const QString &group : settings.childGroups()) {
				settings.beginGroup(group);
				importers<<new ExtCmdImporter(group,
					settings.value("table").toString(),
					settings.value("patterns").toString().split(" "),
					settings.value("command").toString());
				settings.endGroup();
			}
		}
	}
	qDebug()<<"\tFound "<<importers.size()-1<<" importers";
	qDebug()<<"End loading importers ...";
}
