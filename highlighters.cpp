#include "highlighters.h"

#include <QDebug>
#include <QtAlgorithms>

// Adapted from http://doc.qt.io/qt-5/qtwidgets-richtext-syntaxhighlighter-example.html

MySqliteHighlighter::MySqliteHighlighter(QTextEdit *parent): QSyntaxHighlighter(parent) {
	HighlightingRule rule;

	// This one is for the column names. It will be replaced when it is updated.
	columnName.setForeground(Qt::darkRed);
	rule.pattern=REGEX("^$");
	rule.format=columnName;
	highlightingRules.append(rule);
	ruleColumnName=highlightingRules.count()-1;


	keywordFormat.setForeground(Qt::darkBlue);
	keywordFormat.setFontWeight(QFont::Bold);
	QStringList keywordPatterns;
	keywordPatterns<<"SELECT"<<"FROM"<<"WHERE"<<"GROUP BY"<<"DISTINCT"
        <<"ORDER BY"<<"ASC"<<"DESC"<<"AS"<<"AND"<<"OR"<<"NOT"<<"LIKE"
        <<"LIMIT"<<"UNION"<<"UNION ALL"<<"CASE"<<"WHEN"<<"THEN"<<"ELSE"<<"END"
	<<"WINDOW"<<"PARTITION"<<"BY"<<"OVER"<<"INNER JOIN"<<"OUTER JOIN"<<"JOIN"<<"ON";
	foreach (const QString &pattern, keywordPatterns) {
		rule.pattern = REGEX(QString("\\b%1\\b").arg(pattern),REGEX::CaseInsensitiveOption);
		rule.format = keywordFormat;
		highlightingRules.append(rule);
	}

	// Quoted string
	quotationFormat.setForeground(Qt::darkGreen);
	rule.format = quotationFormat;
		rule.pattern = REGEX("'[^']*'"); highlightingRules.append(rule);
		rule.pattern = REGEX("\"[^\"]*\""); highlightingRules.append(rule);

	// Function application
	unknownFunctionFormat.setFontItalic(true);
	unknownFunctionFormat.setFontWeight(QFont::Bold);
	unknownFunctionFormat.setForeground(Qt::blue);
	rule.pattern = REGEX("\\b[A-Za-z0-9_]+(?=\\()");
	rule.format = unknownFunctionFormat;
	highlightingRules.append(rule);

	// Function application
	functionFormat.setFontItalic(true);
	functionFormat.setForeground(Qt::blue);
	rule.pattern = REGEX("("
        "AVG|COUNT|MAX|MAX0|MIN|SUM|TOTAL|"
		"ABS|GLOB|HEX|IFNULL|INSTR|LENGTH|LIKE|LOWER|LTRIM|NULLIF|"
		"PRINTF|QUOTE|RANDOM|ROUND|RTRIM|SUBSTR|TRIM|UPPER|"
		// selection of functions provided by extensions, see libsqlitefunctions.c
        "STDEV|MEDIAN|VARIANCE|MODE|UPPER_QUARTILE|LOWER_QUARTILE"
		")+(?=\\()",QRegularExpression::CaseInsensitiveOption);
	rule.format = functionFormat;
	highlightingRules.append(rule);

	// Unknown variable
	unknownVariableFormat.setForeground(Qt::red);
	unknownVariableFormat.setFontWeight(QFont::ExtraBold);
	rule.pattern = REGEX("\\$\\{[^}]*\\}");
	rule.format = unknownVariableFormat;
	highlightingRules.append(rule);

	// Valid variable
	variableFormat.setForeground(Qt::darkMagenta);
	unknownVariableFormat.setFontWeight(QFont::Normal);
	rule.pattern = REGEX("\\$\\{[^}]*\\}");
	rule.format = variableFormat;
	highlightingRules.append(rule);
	ruleValidVariable=highlightingRules.count()-1;

	// Comment
	singleLineCommentFormat.setForeground(Qt::darkGray);
	rule.pattern = REGEX("#[^\n]*$");
	rule.format = singleLineCommentFormat;
	highlightingRules.append(rule);
}

// We sort by length, so the first match will also match the complete column/variable
int sortByLenDesc(const QString &l, const QString &r) { return l.length()>r.length(); }

void MySqliteHighlighter::setColumnNames(QStringList columnNames) {
	std::sort(columnNames.begin(), columnNames.end(), sortByLenDesc);
	const QString rgx=QString("\\$\\{%1\\}").arg(columnNames.join("|"));
	highlightingRules[ruleColumnName].pattern=REGEX(rgx,REGEX::CaseInsensitiveOption);
}

void MySqliteHighlighter::setVariableNames(QStringList varNames) {
	std::sort(varNames.begin(), varNames.end(), sortByLenDesc);
	const QString rgx=QString("\\$\\{(%1)\\}").arg(varNames.join("|"));
	highlightingRules[ruleValidVariable].pattern=REGEX(rgx,REGEX::CaseInsensitiveOption);
}

void MySqliteHighlighter::highlightBlock(const QString &text)  {
	foreach (const HighlightingRule &rule, highlightingRules) {
		auto matchIterator = rule.pattern.globalMatch(text);
		while (matchIterator.hasNext()) {
			auto match = matchIterator.next();
			setFormat(match.capturedStart(), match.capturedLength(), rule.format);
		}
	}
}






MyBatchSaveHighlighter::MyBatchSaveHighlighter(QTextEdit *parent): QSyntaxHighlighter(parent) {
	HighlightingRule rule;

	// Path
	pathFormat.setForeground(Qt::blue);
	rule.pattern=REGEX(".*/.*[.](png)");
	rule.format=pathFormat;
	highlightingRules.append(rule);

	// Variable assignment
	variableAssignmentFormat.setForeground(Qt::darkRed);
	rule.pattern=REGEX("^[a-zA-Z_0-9-]*\\s*=");
	rule.format=variableAssignmentFormat;
	highlightingRules.append(rule);

	// Variable
	variableFormat.setForeground(Qt::darkGreen);
	rule.pattern = REGEX("\\$\\{[^}]*\\}");
	rule.format = variableFormat;
	highlightingRules.append(rule);

	// Comment
	singleLineCommentFormat.setForeground(Qt::darkGray);
	rule.pattern = REGEX("#[^\n]*");
	rule.format = singleLineCommentFormat;
	highlightingRules.append(rule);

}

void MyBatchSaveHighlighter::highlightBlock(const QString &text)  {
	foreach (const HighlightingRule &rule, highlightingRules) {
		auto matchIterator = rule.pattern.globalMatch(text);
		while (matchIterator.hasNext()) {
			auto match = matchIterator.next();
			setFormat(match.capturedStart(), match.capturedLength(), rule.format);
		}
	}
}
