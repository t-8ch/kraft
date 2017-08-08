/***************************************************************************
             docdigestdetailview.cpp  - Details of a doc digest
                             -------------------
    begin                : februry 2011
    copyright            : (C) 2011 by Klaas Freitag
    email                : freitag@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QtGui>
#include <QDebug>
#include <QHBoxLayout>
#include <QStandardPaths>

#include <klocalizedstring.h>

#include "docdigest.h"
#include "docdigestdetailview.h"

#include "htmlview.h"
#include "texttemplate.h"

DocDigestHtmlView::DocDigestHtmlView( QWidget *parent )
  : HtmlView( parent )
{
    connect(this, SIGNAL(openUrl(QUrl)), this, SLOT(slotLinkClicked(QUrl)));
}

void DocDigestHtmlView::slotLinkClicked(const QUrl& url)
{
    QUrlQuery q(url);
    // Url is like "#show_last_print\\?id=(\\d+)"

    QString idStr = q.queryItemValue(QLatin1String("id"));

    QString path = url.path();
    if( path.endsWith("show_last_print")) {
        bool ok;
        emit( showLastPrint( dbID(idStr.toInt(&ok)) ) );
    }
}

// #########################################################################################################

DocDigestDetailView::DocDigestDetailView(QWidget *parent) :
    QWidget(parent)
{
  QHBoxLayout *hbox = new QHBoxLayout;

  hbox->setMargin(0);
  setLayout( hbox );
  mHtmlCanvas = new DocDigestHtmlView( this );
  mHtmlCanvas->setStylesheetFile("docdigestview.css");

  connect( mHtmlCanvas, SIGNAL(showLastPrint( const dbID& )),
           this, SIGNAL( showLastPrint( const dbID& ) ) );

  hbox->addWidget( mHtmlCanvas);
}

void DocDigestDetailView::slotClearView()
{
    const QString details;
    mHtmlCanvas->displayContent( details );
}

#define DOCDIGEST_TAG

void DocDigestDetailView::slotShowDocDetails( DocDigest digest )
{
  // qDebug () << "Showing details about this doc: " << digest.id();

  if( mTemplFile.isEmpty() ) {
    // QString templFileName = QString( "kraftdoc_%1_ro.trml" ).arg( doc->docType() );
    QString templFileName = QString( "docdigest.trml" );
    QString findFile = "kraft/reports/" + templFileName;

    QString tmplFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, findFile);

    if ( tmplFile.isEmpty() ) {
        QByteArray kraftHome = qgetenv("KRAFT_HOME");

        if( !kraftHome.isEmpty() ) {
            QString file = QString( "%1/reports/docdigest.trml").arg(QString::fromLocal8Bit(kraftHome));
            QFileInfo fi(file);
            if( fi.exists() && fi.isReadable() ) {
                tmplFile = file;
            }
        }
        if( tmplFile.isEmpty() ) {
            // qDebug () << "Could not find template to render document digest.";
            return;
        }
    }
    mTemplFile = tmplFile;
  }

  TextTemplate tmpl( mTemplFile ); // template file with name docdigest.trml
  if( !tmpl.open() ) {
      return;
  }
  tmpl.setValue( DOCDIGEST_TAG( "HEADLINE" ), digest.type() + " " + digest.ident() );

  tmpl.setValue( DOCDIGEST_TAG( "DATE" ), digest.date() );
  tmpl.setValue( DOCDIGEST_TAG( "DATE_LABEL" ), i18n("Date") );

  tmpl.setValue( DOCDIGEST_TAG( "WHITEBOARD"), digest.whiteboard() );
  tmpl.setValue( DOCDIGEST_TAG( "WHITEBOARD_LABEL"), i18n("Whiteboard"));

  if( !digest.projectLabel().isEmpty() ) {
    tmpl.createDictionary( "PROJECT_INFO" );
    tmpl.setValue( "PROJECT_INFO", DOCDIGEST_TAG( "PROJECT"), digest.projectLabel() );
    tmpl.setValue( "PROJECT_INFO", DOCDIGEST_TAG( "PROJECT_LABEL"), i18n("Project"));
  }

  // tmpl.setValue( "URL", mHtmlCanvas->baseURL().prettyUrl());
  tmpl.setValue( DOCDIGEST_TAG( "CUSTOMER_LABEL" ), i18n("Customer"));

  KContacts::Addressee addressee = digest.addressee();
  QString adr = digest.clientAddress();
  adr.replace('\n', "<br/>" );

  tmpl.setValue( DOCDIGEST_TAG("CUSTOMER_ADDRESS_FIELD"),adr );

  QString addressBookInfo;
  if( addressee.isEmpty() ) {
    if( digest.clientId().isEmpty() ) {
      addressBookInfo = i18n("The address is not listed in an address book.");
    } else {
      addressBookInfo = i18n("The client has the address book id %1 but can not found in our address books.").arg(digest.clientId());
    }
  } else {
    addressBookInfo  = i18n("The client can be found in our address books.");
    tmpl.createDictionary( "CLIENT_ADDRESS_SECTION");
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENTID" ), digest.clientId() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_ADDRESS" ), digest.clientAddress() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_NAME"), addressee.realName() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_ORGANISATION"), addressee.organization() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_URL"), addressee.url().toString() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_EMAIL"), addressee.preferredEmail() );

    KContacts::Address clientAddress;
    clientAddress = addressee.address( KContacts::Address::Pref );
    QString addressType = i18n("preferred address");

    if( clientAddress.isEmpty() ) {
      clientAddress = addressee.address( KContacts::Address::Home );
      addressType = i18n("home address");
    }
    if( clientAddress.isEmpty() ) {
      clientAddress = addressee.address( KContacts::Address::Work );
      addressType = i18n("work address");
    }
    if( clientAddress.isEmpty() ) {
      clientAddress = addressee.address( KContacts::Address::Postal );
      addressType = i18n("postal address");
    }
    if( clientAddress.isEmpty() ) {
      clientAddress = addressee.address( KContacts::Address::Intl );
      addressType = i18n("international address");
    }
    if( clientAddress.isEmpty() ) {
      clientAddress = addressee.address( KContacts::Address::Dom );
      addressType = i18n("domestic address");
    }

    if( clientAddress.isEmpty() ) {
      addressType = i18n("unknown");
      // qDebug () << "WRN: Address is still empty!";
    }

    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_POSTBOX" ), clientAddress.postOfficeBox() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_EXTENDED" ), clientAddress.extended() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_STREET" ), clientAddress.street() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_LOCALITY" ), clientAddress.locality() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_REGION" ), clientAddress.region() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_POSTCODE" ), clientAddress.postalCode() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_COUNTRY" ),  clientAddress.country() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_REGION" ), clientAddress.region() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_LABEL" ), clientAddress.label() );
    tmpl.setValue( "CLIENT_ADDRESS_SECTION", DOCDIGEST_TAG( "CLIENT_ADDRESS_TYPE" ), addressType );

  }
  tmpl.setValue( DOCDIGEST_TAG("CUSTOMER_ADDRESSBOOK_INFO"), addressBookInfo );

  // Information about archived documents.
  ArchDocDigestList archDocs = digest.archDocDigestList();
  if( archDocs.isEmpty() ) {
    // qDebug () << "No archived docs for this document!";
    tmpl.createDictionary( DOCDIGEST_TAG( "NEVER_PRINTED" ));
    tmpl.setValue( "NEVER_PRINTED", DOCDIGEST_TAG("ARCHDOCS_TAG"), i18n("This document was never printed."));
  } else {
    ArchDocDigest digest = archDocs[0];
    tmpl.createDictionary("PRINTED");
    tmpl.setValue( "PRINTED", DOCDIGEST_TAG("LAST_PRINT_DATE"), digest.printDate().toString() );
    tmpl.setValue( "PRINTED", DOCDIGEST_TAG("LAST_PRINTED_ID"), digest.archDocId().toString() );
    tmpl.setValue( "PRINTED", DOCDIGEST_TAG("ARCHIVED_COUNT"), QString::number( archDocs.count()-1 ) );
  }

  const QString details = tmpl.expand();
  mHtmlCanvas->displayContent( details );

  // qDebug () << "BASE-URL of htmlview is " << mHtmlCanvas->baseURL();


}
