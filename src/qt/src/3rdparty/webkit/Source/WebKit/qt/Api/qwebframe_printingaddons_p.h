/*
    Copyright (C) 2012 Milian Wolff, KDAB (milian.wolff@kdab.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef QWEBFRAME_PRINTINGADDONS_P_H
#define QWEBFRAME_PRINTINGADDONS_P_H

#include "qwebframe.h"
#include "qwebframe_p.h"

#include <qprinter.h>
#include <qstring.h>

#include "GraphicsContext.h"
#include "PrintContext.h"

// PDF LINKS

#include <QDebug>

class PdfLinks
{
public:
    PdfLinks(const QWebFrame* frame, const WebCore::PrintContext& printCtx);
    ~PdfLinks();

    void paintLinks(int page, QPainter& painter, const WebCore::PrintContext& printCtx);

private:
    void findLinks(const WebCore::PrintContext& printCtx);

    const QWebFrame* m_frame;

    // Anchors are local targets for links
    QHash<QString, QWebElement> anchors;
    // Trigger links to anchors
    QVector<QPair<QWebElement, QString> > links;
    // Links to external resources
    QVector<QPair<QWebElement, QString> > hyperlinks;

	QHash<int, QHash<QString, QWebElement> > pagedAnchors;
	QHash<int, QVector< QPair<QWebElement, QString> > > pagedLinks;
	QHash<int, QVector< QPair<QWebElement, QString> > > pagedHyperlinks;
};

PdfLinks::PdfLinks(const QWebFrame* frame, const WebCore::PrintContext& printCtx)
{
    if (frame) {
        m_frame = frame;

        findLinks(printCtx);
    }
}

PdfLinks::~PdfLinks()
{
}

void PdfLinks::paintLinks(int page, QPainter& painter, const WebCore::PrintContext& printCtx)
{
	// Paint anchors
	for(QHash<QString, QWebElement>::iterator i = pagedAnchors[page].begin(); i != pagedAnchors[page].end(); ++i) {
        qDebug() << "PdfLinks: paint anchor: " << i.key() << ", page" << m_frame->elementLocation(i.value(), printCtx).first;
        QRectF r = m_frame->elementLocation(i.value(), printCtx).second;
		qDebug() << "Location: w=" << r.width() << ", h=" << r.height() << ", x=" << r.x() << ", y=" << r.y();
        painter.addAnchor(r, i.key());
    }

	// Paint local links
    for(QVector< QPair<QWebElement, QString> >::iterator i = pagedLinks[page].begin(); i != pagedLinks[page].end(); ++i) {
        qDebug() << "PdfLinks: paint link: " << i->second << ", page" << m_frame->elementLocation(i->first, printCtx).first;
        QRectF r = m_frame->elementLocation(i->first, printCtx).second;
		qDebug() << "Location: w=" << r.width() << ", h=" << r.height() << ", x=" << r.x() << ", y=" << r.y();
        painter.addLink(r, i->second);
    }

	// Paint hyperlinks
    for(QVector< QPair<QWebElement, QString> >::iterator i = pagedHyperlinks[page].begin(); i != pagedHyperlinks[page].end(); ++i) {
        qDebug() << "PdfLinks: paint hyperlink: " << i->second << ", page" << m_frame->elementLocation(i->first, printCtx).first;
        QRectF r = m_frame->elementLocation(i->first, printCtx).second;
		qDebug() << "Location: w=" << r.width() << ", h=" << r.height() << ", x=" << r.x() << ", y=" << r.y();
        painter.addHyperlink(r, QUrl(i->second));
    }
}

void PdfLinks::findLinks(const WebCore::PrintContext& printCtx)
{
    QWebElementCollection elements = m_frame->findAllElements("a");
    foreach(const QWebElement element, elements) {
        QString name = element.attribute("name");
        if (name.size() > 0) {
            anchors[name] = element;

            qDebug() << "PdfLinks: Found New Anchor: " << name;
        }

        QString href = element.attribute("href");
        if (href.startsWith("#")) {
            href = href.remove(0, 1); //remove the #

            if (href.size() > 0) {
                QWebElement e = m_frame->findAnchor(href);
                if (!e.isNull()) {
                    anchors[href] = e;
                }
                // Add the link anyway
                links.push_back(qMakePair(element, href));

                qDebug() << "PdfLinks: Found New Link: " << href;
            }
        } else {
            // Hyperlink link to URL
            QUrl url(href);
            if (url.isEmpty())
                continue;

            url = m_frame->baseUrl().resolved(url);

            // check if this URL points to our web page, if true, this is a local link
            if (m_frame->url().toString() == url.toString(QUrl::RemoveFragment)) {
                QWebElement e;
                if (!url.hasFragment()) {
                    e = m_frame->findFirstElement("body");
                } else {
                    e = m_frame->findAnchor(url.fragment());
                }

                if (!e.isNull()) {
                    // Add this to anchors, because not only <a> elements can be anchors
                    // and we most probably missed this anchor
                    anchors[url.toString()] = e;
                    // Add this to local links also, to point to our anchor
                    links.push_back(qMakePair(element, url.toString()));

                    qDebug() << "PdfLinks: Found Anchor & Link: " << url.toString();
                }
            } else {
                hyperlinks.push_back(qMakePair(element, href));

                qDebug() << "PdfLinks: Found New Hyperlink: " << href;
            }
        }
    }

	//Sort anchors and links by page

	for (QHash<QString, QWebElement>::iterator i=anchors.begin(); i != anchors.end(); ++i)
		pagedAnchors[m_frame->elementLocation(i.value(), printCtx).first][i.key()] = i.value();

	for (QVector< QPair<QWebElement, QString> >::iterator i=links.begin(); i != links.end(); ++i)
		pagedLinks[m_frame->elementLocation(i->first, printCtx).first].push_back(*i);

	for (QVector< QPair<QWebElement, QString> >::iterator i=hyperlinks.begin(); i != hyperlinks.end(); ++i)
		pagedHyperlinks[m_frame->elementLocation(i->first, printCtx).first].push_back(*i);
}

// END PDF LINKS

// for custom header or footers in printing
class HeaderFooter
{
public:
    HeaderFooter(const QWebFrame* frame, QPrinter* printer, QWebFrame::PrintCallback* callback);
    ~HeaderFooter();

    void setPageRect(const WebCore::IntRect& rect);

    void paintHeader(WebCore::GraphicsContext& ctx, const WebCore::IntRect& pageRect, int pageNum, int totalPages);
    void paintFooter(WebCore::GraphicsContext& ctx, const WebCore::IntRect& pageRect, int pageNum, int totalPages);

    bool isValid()
    {
        return callback && (headerHeightPixel > 0 || footerHeightPixel > 0);
    }

private:
    QWebPage page;
    QWebFrame::PrintCallback* callback;
    int headerHeightPixel;
    int footerHeightPixel;

    WebCore::PrintContext* printCtx;

    void paint(WebCore::GraphicsContext& ctx, const WebCore::IntRect& pageRect, const QString& contents, int height);
};

HeaderFooter::HeaderFooter(const QWebFrame* frame, QPrinter* printer, QWebFrame::PrintCallback* callback_)
: printCtx(0)
, callback(callback_)
, headerHeightPixel(0)
, footerHeightPixel(0)
{
    if (callback) {
        qreal headerHeight = qMax(qreal(0), callback->headerHeight());
        qreal footerHeight = qMax(qreal(0), callback->footerHeight());

        if (headerHeight || footerHeight) {
            // figure out the header/footer height in *DevicePixel*
            // based on the height given in *Points*
            qreal marginLeft, marginRight, marginTop, marginBottom;
            // find existing margins
            printer->getPageMargins(&marginLeft, &marginTop, &marginRight, &marginBottom, QPrinter::DevicePixel);
            const qreal oldMarginTop = marginTop;
            const qreal oldMarginBottom = marginBottom;

            printer->getPageMargins(&marginLeft, &marginTop, &marginRight, &marginBottom, QPrinter::Point);
            // increase margins to hold header+footer
            marginTop += headerHeight;
            marginBottom += footerHeight;
            printer->setPageMargins(marginLeft, marginTop, marginRight, marginBottom, QPrinter::Point);

            // calculate actual heights
            printer->getPageMargins(&marginLeft, &marginTop, &marginRight, &marginBottom, QPrinter::DevicePixel);
            headerHeightPixel = marginTop - oldMarginTop;
            footerHeightPixel = marginBottom - oldMarginBottom;

            printCtx = new WebCore::PrintContext(QWebFramePrivate::webcoreFrame(page.mainFrame()));
        }
    }
}

HeaderFooter::~HeaderFooter()
{
    delete printCtx;
    printCtx = 0;
}

void HeaderFooter::paintHeader(WebCore::GraphicsContext& ctx, const WebCore::IntRect& pageRect, int pageNum, int totalPages)
{
    if (!headerHeightPixel) {
        return;
    }
    const QString c = callback->header(pageNum, totalPages);
    if (c.isEmpty()) {
        return;
    }

    ctx.translate(0, -headerHeightPixel);
    paint(ctx, pageRect, c, headerHeightPixel);
    ctx.translate(0, +headerHeightPixel);
}

void HeaderFooter::paintFooter(WebCore::GraphicsContext& ctx, const WebCore::IntRect& pageRect, int pageNum, int totalPages)
{
    if (!footerHeightPixel) {
        return;
    }
    const QString c = callback->footer(pageNum, totalPages);
    if (c.isEmpty()) {
        return;
    }

    const int offset = pageRect.height();
    ctx.translate(0, +offset);
    paint(ctx, pageRect, c, footerHeightPixel);
    ctx.translate(0, -offset);
}

void HeaderFooter::paint(WebCore::GraphicsContext& ctx, const WebCore::IntRect& pageRect, const QString& contents, int height)
{
    page.mainFrame()->setHtml(contents);

    printCtx->begin(pageRect.width(), height);
    float tempHeight;
    printCtx->computePageRects(pageRect, /* headerHeight */ 0, /* footerHeight */ 0, /* userScaleFactor */ 1.0, tempHeight);

    printCtx->spoolPage(ctx, 0, pageRect.width());

    printCtx->end();
}


#endif // QWEBFRAME_PRINTINGADDONS_P_H
