/*
 * This file is part of BeamerPresenter.
 * Copyright (C) 2019  stiglers-eponym

 * BeamerPresenter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * BeamerPresenter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with BeamerPresenter. If not, see <https://www.gnu.org/licenses/>.
 */

#include "presentationscreen.h"

PresentationScreen::PresentationScreen(PdfDoc* presentationDoc, QWidget *parent) : QWidget(parent)
{
    presentation = presentationDoc;
    setGeometry(0, 0, 1920, 1080);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setMinimumSize(120, 90);
    QPalette palette = QPalette();
    palette.setColor(QPalette::Window, QPalette::Shadow);
    setPalette(palette);

    // label will contain the slide as a pixmap
    label = new PageLabel(this);
    label->setAlignment(Qt::AlignCenter);
    layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget( label, 0, 0 );
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(label, &PageLabel::sendNewPageNumber,       this, &PresentationScreen::receiveNewPageNumber);
    connect(label, &PageLabel::sendNewPageNumber,       this, &PresentationScreen::sendNewPageNumber);
    connect(label, &PageLabel::timeoutSignal,           this, &PresentationScreen::receiveTimeoutSignal);
    connect(this, &PresentationScreen::togglePointerVisibilitySignal, label, &PageLabel::togglePointerVisibility);
    label->togglePointerVisibility();
    show();
}

PresentationScreen::~PresentationScreen()
{
    label->disconnect();
    //delete presentation;
    disconnect();
    delete label;
    delete layout;
}

PageLabel * PresentationScreen::getLabel()
{
    return label;
}

int PresentationScreen::getPageNumber() const
{
    return label->pageNumber();
}

void PresentationScreen::renderPage( const int pageNumber )
{
    if ( pageNumber < 0 || pageNumber >= presentation->getDoc()->numPages() )
        label->renderPage( presentation->getPage( presentation->getDoc()->numPages() - 1 ) );
    else
        label->renderPage( presentation->getPage(pageNumber) );
}

void PresentationScreen::updateCache()
{
    int pageNumber = label->pageNumber() + 1;
    if (pageNumber < presentation->getDoc()->numPages())
        label->updateCache( presentation->getPage(pageNumber) );
}

void PresentationScreen::receiveTimeoutSignal()
{
    renderPage( label->pageNumber() + 1 );
    if ( label->getDuration() < 0 || label->getDuration() > 0.5 )
        emit sendPageShift();
}

void PresentationScreen::receiveNewPageNumber( const int pageNumber )
{
    renderPage(pageNumber);
}

void PresentationScreen::receiveCloseSignal()
{
    close();
}

void PresentationScreen::keyPressEvent( QKeyEvent * event )
{
    // TODO: Find a nicer way to do this
    switch ( event->key() ) {
        case Qt::Key_Right:
        case Qt::Key_PageDown:
            renderPage( label->pageNumber() + 1 );
            if ( label->getDuration() < 0 || label->getDuration() > 0.5 )
                emit sendPageShift();
            updateCache();
            if ( label->getDuration() < 0 || label->getDuration() > 0.5 )
                emit sendUpdateCache();
            break;
        case Qt::Key_Left:
        case Qt::Key_PageUp:
            {
                int page = label->pageNumber() - 1;
                if (page >= 0) {
                    renderPage( label->pageNumber() - 1 );
                    if ( label->getDuration() < 0 || label->getDuration() > 0.5 )
                        emit sendPageShift();
                    updateCache();
                    if ( label->getDuration() < 0 || label->getDuration() > 0.5 )
                        emit sendUpdateCache();
                }
            }
            break;
        //case Qt::Key_E: // For those who prefere vim shortcuts
        case Qt::Key_Down:
            {
                int pageNumber = presentation->getNextSlideIndex(label->pageNumber());
                renderPage( pageNumber );
                emit sendNewPageNumber( pageNumber );
                updateCache();
                emit sendUpdateCache();
            }
            break;
        //case Qt::Key_B: // For those who prefere vim shortcuts
        case Qt::Key_Up:
            {
                int pageNumber = presentation->getPreviousSlideEnd(label->pageNumber());
                renderPage( pageNumber );
                emit sendNewPageNumber( pageNumber );
                updateCache();
                emit sendUpdateCache();
            }
            break;
        case Qt::Key_Space:
            renderPage( label->pageNumber() );
            emit sendPageShift();
            updateCache();
            emit sendUpdateCache();
            break;
        case Qt::Key_O:
            emit togglePointerVisibilitySignal();
            break;
        case Qt::Key_F:
        case Qt::Key_F11:
            if (this->windowState() == Qt::WindowFullScreen)
                showNormal();
            else
                showFullScreen();
            break;
        default:
            emit sendKeyEvent(event);
            break;
    }
    event->accept();
}

void PresentationScreen::resizeEvent(QResizeEvent *event)
{
    label->clearCache();
    label->renderPage( label->getPage() );
}
