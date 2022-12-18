#pragma once

#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>


class AspectRatioPixmapLabel: public QLabel {
	Q_OBJECT
public:
	explicit AspectRatioPixmapLabel(bool keepAR, QWidget *parent = nullptr);

	virtual int heightForWidth( int width ) const override;
	virtual QSize sizeHint() const override;

	void setKeepAspectRatio(bool keep);
	bool keepAspectRatio() const { return _keepAR; }
	void setCustomPixmap(const QPixmap &);
	void showCrossHair(bool show);

	// Used to determine how to draw the cross-hair.
	QString plot_type;
	double d3Xrot, d3Zrot; // if plot_type=='splot', these are used to determine the orientation
	void repaintAll() const;
	void save(const QString &fname) const;
	QPixmap scaledPixmap() const;
private:
	void resizeEvent(QResizeEvent *) override;
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *ev) override;
	void mouseMoveEvent(QMouseEvent *ev) override;
	void mouseReleaseEvent(QMouseEvent *ev) override;
private:
	void doIt();

	QString label() const;

	bool _showCH=false;
	bool _keepAR=false;
	QPixmap pix;
};
