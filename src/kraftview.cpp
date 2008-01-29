/***************************************************************************
                          kraftview.cpp  -
                             -------------------
    begin                : Mit Dez 31 19:24:05 CET 2003
    copyright            : (C) 2003 by Klaas Freitag
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

// include files for Qt
#include <qlayout.h>
#include <qlabel.h>
#include <qscrollview.h>
#include <qsizepolicy.h>
#include <qtextedit.h>
#include <qsignalmapper.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qgrid.h>
#include <qwidgetstack.h>
#include <qtabwidget.h>
#include <qcolor.h>
#include <qsplitter.h>
#include <qbuttongroup.h>
#include <qtooltip.h>
#include <qfont.h>

#include <kdebug.h>
#include <kdialogbase.h>
#include <kpushbutton.h>
#include <kcombobox.h>
#include <kdatewidget.h>
#include <knuminput.h>
#include <kactioncollection.h>
#include <kmessagebox.h>
#include <khtmlview.h>
#include <kiconloader.h>

#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>
#include <kabc/addresseedialog.h>
#include <kabc/addressee.h>


// application specific includes
#include "kraftdb.h"
#include "kraftsettings.h"
#include "kraftview.h"
#include "kraftdoc.h"
#include "portal.h"
#include "docheader.h"
#include "docassistant.h"
#include "positionviewwidget.h"
#include "docfooter.h"
#include "docposition.h"
#include "unitmanager.h"
#include "docpostcard.h"
#include "kataloglistview.h"
#include "katalogman.h"
#include "templkatalog.h"
#include "templkataloglistview.h"
#include "catalogselection.h"
#include "headerselection.h"
#include "kraftdocheaderedit.h"
#include "kraftdocpositionsedit.h"
#include "kraftdocfooteredit.h"
#include "inserttempldialog.h"
#include "defaultprovider.h"
#include "stockmaterial.h"
#include "brunsrecord.h"
#include "insertplantdialog.h"
#include "templtopositiondialogbase.h"
#include "doctype.h"
#include "catalogtemplate.h"

#include <qtimer.h>


KraftViewScroll::KraftViewScroll( QWidget *parent ):
QScrollView( parent )
{

}

void KraftViewScroll::viewportResizeEvent ( QResizeEvent *ev )
{
  int w = ev->size().width(); // visibleWidth()-1;
  resizeContents( w, contentsHeight () );
  QScrollView::viewportResizeEvent( ev );
}

void KraftViewScroll::resizeContents(  int w, int h )
{
  if( w < 400 ) {
    w = 400;
  }
  QScrollView::resizeContents( w, h );
  PositionViewWidget *wid;
  for ( wid = mWidgetList.first(); wid; wid = mWidgetList.next() ) {
    wid->resize( w, wid->height() );
  }
}

void KraftViewScroll::addChild( QWidget *child, int x, int y )
{
  mWidgetList.append( static_cast<PositionViewWidget*>(child) );
  QScrollView::addChild( child, x, y );
}

void KraftViewScroll::kraftRemoveChild( PositionViewWidget *child )
{
  removeChild( child ); // from the scrollview
  mWidgetList.removeRef( child );
}

// #########################################################

KraftView::KraftView(QWidget *parent, const char *name) :
  KDialogBase( parent, name, false /* modal */, i18n("Document"),
	      Ok|Cancel, Ok, true /* separator */ ),
  m_doc( 0 ),  mRememberAmount( -1 ),
  mModified( false )
{
  mDeleteMapper = new QSignalMapper( this );
  connect( mDeleteMapper, SIGNAL( mapped(int)),
           this, SLOT( slotDeletePosition( int ) ) );

  mMoveUpMapper = new QSignalMapper( this );
  connect( mMoveUpMapper, SIGNAL( mapped(int)),
           this, SLOT( slotMovePositionUp( int  ) ) );

  mMoveDownMapper = new QSignalMapper( this );
  connect( mMoveDownMapper, SIGNAL( mapped(int)),
           this, SLOT( slotMovePositionDown( int ) ) );

  mLockPositionMapper = new QSignalMapper( this );
  connect( mLockPositionMapper, SIGNAL( mapped( int )),
           this, SLOT( slotLockPosition( int ) ) );

  mUnlockPositionMapper = new QSignalMapper( this );
  connect( mUnlockPositionMapper, SIGNAL( mapped( int )),
           this, SLOT( slotUnlockPosition( int ) ) );

  mModifiedMapper = new QSignalMapper( this );
  connect( mModifiedMapper,  SIGNAL( mapped( int ) ),
           this,  SLOT( slotPositionModified( int ) ) );

#if 0
  connect( this, SIGNAL( aboutToShowPage( QWidget* ) ),
           this, SLOT( slotAboutToShow( QWidget* ) ) );
#endif

  mGlobalVBox = makeVBoxMainWidget();
  mGlobalVBox->setMargin( 3 );

  mDetailHeader = new QLabel( mGlobalVBox );
  mDetailHeader->setSizePolicy( QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Fixed ) );
  // mDetailHeader->setMargin( 4 );
  mDetailHeader->setFrameStyle( QFrame::Box + QFrame::Plain );
  mDetailHeader->setLineWidth( 1 );
  mDetailHeader->setPaletteBackgroundColor( QColor( "darkBlue" ));
  mDetailHeader->setPaletteForegroundColor( QColor( "white" ) );
  mDetailHeader->setTextFormat( Qt::PlainText );
  mDetailHeader->setFixedHeight( 40 ); // FIXME
  QFont f = mDetailHeader->font();
  f.setPointSize( qRound( 1.4 * f.pointSize() ) );
  f.setBold( true );
  mDetailHeader->setFont( f );

  mCSplit    = new QSplitter( mGlobalVBox );
  QVBox *vb  = new QVBox( mCSplit );
  vb->setMargin( 3 );
  mViewStack = new QWidgetStack( vb );
  mViewStack->setMargin( 0 );
  kdDebug() << "mViewSTack height is " << mViewStack->height() << endl;

  mAssistant = new DocAssistant( mCSplit );
  /* catalog template selection signal */
  connect( mAssistant,  SIGNAL( positionSelected( Katalog*, void* ) ),
           this,  SLOT( slotAddPosition( Katalog*, void* ) ) );

  /* signal to toggle the visibility of the template section in the assistant */
  connect(  mAssistant, SIGNAL( toggleShowTemplates( bool ) ),
            this,  SLOT( slotShowTemplates( bool ) ) );

  /* signal that brings a new address to the document */
  connect( mAssistant, SIGNAL( addressTemplate( const Addressee& ) ),
           this, SLOT( slotNewAddress( const Addressee& ) ) );

  connect( mAssistant, SIGNAL( headerTextTemplate( const QString& ) ),
           this, SLOT( slotNewHeaderText( const QString& ) ) );

  connect( mAssistant, SIGNAL( footerTextTemplate( const QString& ) ),
           this, SLOT( slotNewFooterText( const QString& ) ) );

  if ( KraftSettings::self()->docViewSplitter().count() == 2 ) {
    mCSplit->setSizes( KraftSettings::self()->docViewSplitter() );
  }
  connect( mAssistant, SIGNAL( selectPage( int ) ),
           this,  SLOT( slotSwitchToPage( int ) ) );

  QSize size = KraftSettings::self()->docViewSize();
  if ( !size.isEmpty() ) resize( size );
  QPoint pos = KraftSettings::self()->docViewPosition();
  if ( !pos.isNull() ) move( pos );

  mAssistant->slotSelectDocPart( KraftDoc::Header );

  mNewTemplates.setAutoDelete( true );
}

KraftView::~KraftView()
{

}

void KraftView::setup( DocGuardedPtr doc )
{
  m_doc = doc;

  setupDocHeaderView();
  setupPositions();
  setupFooter();
  setCaption( m_doc->docIdentifier() );
  slotSwitchToPage( KraftDoc::Header );
}


void KraftView::slotSwitchToPage( int id )
{
  // check if the wanted part is already visible
  if ( mViewStack->visibleWidget() == mViewStack->widget( id ) ) return;


  mViewStack->raiseWidget( id );

  KraftDocEdit *edit =
    static_cast<KraftDocEdit *>( mViewStack->visibleWidget() );

  mDetailHeader->setText( edit->title() );
  mDetailHeader->setPaletteBackgroundColor( edit->color() );
  // FIXME: color
  mDetailHeader->setPaletteForegroundColor( QColor( "#00008b" ) );

  mAssistant->postCard()->renderDoc( mViewStack->id( mViewStack->visibleWidget() ) );
}

void KraftView::slotShowTemplates( bool )
{
}

void KraftView::setupDocHeaderView()
{
    KraftDocHeaderEdit *edit = new KraftDocHeaderEdit( mainWidget() );

    mHeaderId = mViewStack->addWidget( edit, KraftDoc::Header );

    m_headerEdit = edit->docHeaderEdit();

    m_headerEdit->m_cbType->clear();
    // m_headerEdit->m_cbType->insertStringList( DefaultProvider::self()->docTypes() );
    m_headerEdit->m_cbType->insertStringList( DocType::allLocalised() );

    connect( m_headerEdit->m_cbType,  SIGNAL( activated( const QString& ) ),
             this, SLOT( slotDocTypeChanged( const QString& ) ) );

    connect( edit, SIGNAL( modified() ),
              this, SLOT( slotModifiedHeader() ) );
}

void KraftView::setupPositions()
{
    KraftDocPositionsEdit *edit = new KraftDocPositionsEdit( mainWidget() );
    mViewStack->addWidget( edit, KraftDoc::Positions );

    m_positionScroll = edit->positionScroll();

    connect( edit, SIGNAL( addPositionClicked() ), SLOT( slotAddPosition() ) );
}

void KraftView::redrawDocument( )
{
    KraftDoc *doc = getDocument();
    if( !doc ) {
      kdDebug() << "ERR: No document available in view, return!" << endl;
    } else {
      kdDebug() << "** Starting redraw of document " << doc << endl;
    }

    /* header: date and document type */
    QDate date = doc->date();
    m_headerEdit->m_dateEdit->setDate( date );
    m_headerEdit->m_cbType->setCurrentText( doc->docType() );

    /* header: address */
    mContactUid  = doc->addressUid();
    QString address = doc->address();

    kdDebug() << "Loaded address uid from database " << mContactUid << endl;
    if( ! mContactUid.isEmpty() ) {
      // FIXME - use centralised address provider
      AddressBook *adrBook =  StdAddressBook::self( );
      Addressee contact;
      if( adrBook ) {
        contact = adrBook->findByUid( mContactUid );
        if( contact.isEmpty() ) {
          // the Uid is set, but the address is not in this addressbook
          KMessageBox::sorry( this, i18n("The addressbook does not contain the address "
                                         "identified by the ID in the document.")
                              .arg( mContactUid ), i18n("Addressbook out of sync") );

          m_headerEdit->m_labName->setText( i18n("--lost--") );
        } else {
          kdDebug() << "The loaded Contact has this realname: " << contact.realName() << endl;
          slotNewAddress( contact );
        }
      }
    }
    if( !address.isEmpty() ) {
      // custom address stored in the document.
      // kdDebug() << "custom address came in: " << address << endl;
      m_headerEdit->m_postAddressEdit->setText( address );
    }

    if( !doc->salut().isEmpty() ) {
      m_headerEdit->m_letterHead->insertItem( doc->salut() );
      m_headerEdit->m_letterHead->setCurrentText( doc->salut() );
    }
    /* pre- and post text */
    m_headerEdit->m_teEntry->setText( doc->preText() );
    m_headerEdit->m_whiteboardEdit->setText( doc->whiteboard() );
    m_footerEdit->m_teSummary->setText( doc->postText() );

    mAssistant->slotSetDocType( doc->docType() );
    if ( !doc->goodbye().isEmpty() ) {
      m_footerEdit->m_cbGreeting->setCurrentText( doc->goodbye() );
    }

    redrawDocPositions( );
    slotDocTypeChanged( doc->docType() );
    refreshPostCard();

    mModified = false;
}

void KraftView::redrawDocPositions( )
{
    KraftDoc *doc = getDocument();
    kdDebug() << "** Starting to redraw the positions" << endl;

    if( ! doc ) {
      kdError() << "Empty document pointer" << endl;
      return;
    }

    DocPositionList list = doc->positions();
    kdDebug() << "starting to redraw " << list.count() << " positions!" << endl;

    DocPositionBase *dp = 0;

    for( dp = list.first(); dp; dp = list.next() ) {
        PositionViewWidget *w = mPositionWidgetList.widgetFromPosition( dp );
        if( !w ) {
          w = createPositionViewWidget( dp, list.at() );
        }
        kdDebug() << "now position " << dp->position() << endl;
    }

    // now go through the positionWidgetMap and check if it contains elements
    // that are not any longer in the list of positions
    PositionViewWidget *w = 0;
    for( w = mPositionWidgetList.first(); w; w = mPositionWidgetList.next() ) {
      if( list.containsRef( w->position() ) == 0 ) {
        kdDebug() << "Removing this one: " << w << endl;
        m_positionScroll->kraftRemoveChild( w );
        mPositionWidgetList.remove();
        break;
      }
    }

    // repaint everything
    m_positionScroll->updateContents();

    redrawSumBox();
}

void KraftView::setMappingId( QWidget *widget, int pos )
{
  mDeleteMapper->removeMappings( widget );
  mDeleteMapper->setMapping( widget, pos );

  mMoveUpMapper->removeMappings( widget );
  mMoveUpMapper->setMapping( widget, pos );

  mMoveDownMapper->removeMappings( widget );
  mMoveDownMapper->setMapping( widget, pos );

  mLockPositionMapper->removeMappings( widget );
  mLockPositionMapper->setMapping( widget, pos );

  mUnlockPositionMapper->removeMappings( widget );
  mUnlockPositionMapper->setMapping( widget, pos );

  mModifiedMapper->removeMappings( widget );
  mModifiedMapper->setMapping( widget, pos );

}

PositionViewWidget *KraftView::createPositionViewWidget( DocPositionBase *dp, int pos )
{
  PositionViewWidget *w = new PositionViewWidget( );

  int cw = m_positionScroll->contentsWidth();
  if ( cw < 400 ) cw = 400;
  w->resize( cw, w->height() );

  connect( w, SIGNAL( deletePosition() ),  mDeleteMapper, SLOT( map() ) );
  connect( w, SIGNAL( moveUp() ),          mMoveUpMapper, SLOT( map() ) );
  connect( w, SIGNAL( moveDown() ),        mMoveDownMapper, SLOT( map() ) );
  connect( w, SIGNAL( lockPosition() ),    mLockPositionMapper, SLOT( map() ) );
  connect( w, SIGNAL( unlockPosition() ),  mUnlockPositionMapper, SLOT( map() ) );
  connect( w, SIGNAL( positionModified()), mModifiedMapper,  SLOT( map() ) );

  setMappingId( w, pos );

  connect( w, SIGNAL( positionModified() ), this,
           SLOT( slotModifiedPositions() ) );

  connect( w, SIGNAL( priceChanged( const Geld& ) ), this,
           SLOT( redrawSumBox() ) );
  w->m_cbUnit->insertStringList( UnitManager::allUnits() );

  // kdDebug() << "Adding a widget for position number " << cnt << endl;

  kdDebug() << "Inserting in the new position at " << pos << endl;
  if( dp->dbId().toInt() < 0 ) {
    kdDebug() << "setting state to NEW" << endl;
    w->slotSetState( PositionViewWidget::New );
  }
  mPositionWidgetList.insert( pos,  w );

  /* do resizing and add the widget to the scrollview and move it to the final place */
  m_positionScroll->resizeContents( cw,
                                    mPositionWidgetList.count() * w->height()+1 );

  /* If the new pos is not appended, we have to move all the following on height unit down */
  if ( ( unsigned )pos != mPositionWidgetList.count() ) {
    PositionViewWidget *vw;
    for ( vw = mPositionWidgetList.at( pos+1 ); vw; vw = mPositionWidgetList.next() ) {
      m_positionScroll->moveChild( vw, 0, m_positionScroll->childY( vw ) + w->height() );
    }
  }

  m_positionScroll->addChild( w, 0, 0 );
  w->setDocPosition( dp, getDocument()->locale() );
  w->setOrdNumber( 1 + pos );
  int y = pos * w->height();
  m_positionScroll->moveChild( w, 0, y );
  w->show();

  return w;
}

void KraftView::refreshPostCard()
{
  if ( mAssistant->postCard() ) {
    QDate d = m_headerEdit->m_dateEdit->date();
    const QString dStr = getDocument()->locale()->formatDate( d );

    mAssistant->postCard()->setHeaderData( m_headerEdit->m_cbType->currentText(),
                                           dStr, m_headerEdit->m_postAddressEdit->text(),
                                           getDocument()->ident(),
                                           m_headerEdit->m_teEntry->text() );

    mAssistant->postCard()->setPositions( currentPositionList() );

    mAssistant->postCard()->setFooterData( m_footerEdit->m_teSummary->text(),
                                           m_footerEdit->m_cbGreeting->currentText() );

    mAssistant->postCard()->renderDoc( mViewStack->id( mViewStack->visibleWidget() ) );
  }
}

void KraftView::redrawSumBox()
{
  Geld netto = mPositionWidgetList.nettoPrice();
}

void KraftView::setupFooter()
{
    KraftDocFooterEdit *edit = new KraftDocFooterEdit( mainWidget() );

    mViewStack->addWidget( edit, KraftDoc::Footer );

    m_footerEdit = edit->docFooterEdit();

    m_footerEdit->m_cbGreeting->insertStringList( KraftDB::self()->wordList( "greeting" ) );

    m_footerEdit->m_cbGreeting->setCurrentText( KraftSettings::self()->greeting() );

    connect( edit, SIGNAL( modified() ),
             this, SLOT( slotModifiedFooter() ) );
}

void KraftView::slotAboutToShow( QWidget* w )
{
  kdDebug() << "showing page " << w << endl;
}

/* This is the flow in the move up method:
         0    Bla1          0 Bla1                         0 Bla1
         1    Bla2 <- w2    1 Bla2   -> insert at pos-1 => 1 Bla3
 pos ->  2    Bla3 <- w1    2 Bla4                         2 Bla2
         3    Bla4                                         3 Bla4
 */
void KraftView::slotMovePositionUp( int pos )
{
  PositionViewWidget *w1 = 0;
  PositionViewWidget *w2 = 0;

  kdDebug() << "Moving position up: " << pos << endl;
  if( pos < 1 || (unsigned) pos > mPositionWidgetList.count() ) {
    kdDebug() << "ERR: position out of range: " << pos << endl;
    return;
  }

  w2 = mPositionWidgetList.at( pos-1 );
  w1 = mPositionWidgetList.take( pos );
  mPositionWidgetList.insert( pos-1, w1 );
  kdDebug() << "Found at pos " << pos << " the widgets " << w1 << " and " << w2 << endl;

  PositionViewWidget *vw = 0;
  for( vw = mPositionWidgetList.first(); vw; vw = mPositionWidgetList.next() ) {
    DocPositionBase* pb = vw->position();
    DocPosition *pos = static_cast<DocPosition*>(pb);
    if( ! pos ) {
      kdDebug() << "There is no position!" << endl;
    } else {
      kdDebug() << "Pos " << vw->ordNumber() << ": " << pos->text() << endl;
    }
  }

  if( w1 && w2 ) {
    w1->setOrdNumber( pos );  // note: ordnumbers start with 1, thus add one
    w2->setOrdNumber( pos+1 );
    setMappingId( w1, pos-1 );
    setMappingId( w2, pos );
    // int tmpX = m_positionScroll->childX( w1 );
    int tmpY = m_positionScroll->childY( w1 );

    m_positionScroll->moveChild( w1, 0, m_positionScroll->childY( w2 ) );
    m_positionScroll->moveChild( w2, 0, tmpY );

    refreshPostCard();
  } else {
    kdDebug() << "ERR: Did not find the two corresponding widgets!" << endl;
  }
}
/*
          0    Bla1          0 Bla1                         0 Bla1
  pos ->  1    Bla2 <- w1    1 Bla3                         1 Bla3
          2    Bla3 <- w2    2 Bla4   -> insert at pos+1 => 2 Bla2
          3    Bla4                                         3 Bla4
*/
void KraftView::slotMovePositionDown( int pos )
{
  PositionViewWidget *w1 = 0;
  PositionViewWidget *w2 = 0;
  kdDebug() << "Moving position down: " << pos << endl;

  if( pos < 0 || (unsigned) pos > mPositionWidgetList.count() -1 ) {
    kdDebug() << "ERR: position out of range: " << pos << endl;
  }

  w2 = mPositionWidgetList.at( pos+1 );
  w1 = mPositionWidgetList.take( pos );
  mPositionWidgetList.insert( pos+1, w1 );

  if( w1 && w2 ) {
    w1->setOrdNumber( pos+2  );
    w2->setOrdNumber( pos+1 );

    setMappingId( w1, pos+1 );
    setMappingId( w2, pos );

    int tmpY = m_positionScroll->childY( w1 );
    m_positionScroll->moveChild( w1, 0, m_positionScroll->childY( w2 ) );
    m_positionScroll->moveChild( w2, 0, tmpY );

    refreshPostCard();
  } else {
    kdDebug() << "ERR: Did not find the two corresponding widgets!" << endl;
  }
}

void KraftView::slotDeletePosition( int pos )
{
  PositionViewWidget *w1 = mPositionWidgetList.at( pos );
  if( w1 ) {
    w1->slotSetState( PositionViewWidget::Deleted );

    refreshPostCard();
  }
}

void KraftView::slotLockPosition( int pos )
{
  kdDebug() << "Locking Position " << pos << endl;

  PositionViewWidget *w1 = mPositionWidgetList.at( pos );
  if( w1 ) {
    w1->slotSetState( PositionViewWidget::Locked );
    refreshPostCard();
  }
}

void KraftView::slotUnlockPosition( int pos )
{
  kdDebug() << "Unlocking Position " << pos << endl;

  PositionViewWidget *w1 = mPositionWidgetList.at( pos );
  if( w1 ) {
    w1->slotSetState( PositionViewWidget::Active );
    refreshPostCard();
  }
}

void KraftView::slotPositionModified( int pos )
{
  kdDebug() << "Modified Position " << pos << endl;
  mModified = true;
  QTimer::singleShot( 0, this, SLOT( refreshPostCard() ) );
}

void KraftView::slotNewAddress( const Addressee& contact )
{
  Addressee adr( contact );

  if( contact.isEmpty() ) {
    return;
  }

  if( ! adr.isEmpty() ) {
    m_headerEdit->m_labName->setText( adr.realName() );

    Address address;

    Address::List addresses = adr.addresses();
    if ( addresses.count() > 1 ) {
      kdDebug() << "Have more than one address, taking the default add" << endl;
      address = adr.address( 64 );
    } else if ( addresses.count() == 0 ) {
      kdDebug() << "Have no address, problem!" << endl;
    } else {
      address = addresses.first();
    }

    mContactUid = contact.uid();
    QString adrStr = address.street() + "\n" + address.postalCode();
    adrStr = address.formattedAddress( adr.realName() );
    kdDebug() << "formatted address string: " << adrStr << endl;
    m_headerEdit->m_postAddressEdit->setText( adrStr );
    m_headerEdit->m_letterHead->clear();
    QStringList li = generateLetterHead( adr );

    m_headerEdit->m_letterHead->insertStringList( li );
    m_headerEdit->m_letterHead->setCurrentItem( KraftSettings::self()->salut() );
  }
}

void KraftView::slotDocTypeChanged( const QString& newType )
{
  kdDebug() << "Doc Type changed to " << newType << endl;
  mAssistant->slotSetDocType( newType );

  DocType docType( newType );

  PositionViewWidget *w = 0;

  for ( w = mPositionWidgetList.first(); w; w = mPositionWidgetList.next() ) {
    w->slotEnableKindMenu( docType.allowAlternative() );
  }

}

void KraftView::slotNewHeaderText( const QString& str )
{
  m_headerEdit->m_teEntry->setText( str );
  slotModifiedHeader();
}

void KraftView::slotNewFooterText( const QString& str )
{
  m_footerEdit->m_teSummary->setText( str );
  slotModifiedFooter();
}

void KraftView::slotAddPosition()
{
  // find the katalog
  CatalogSelection *catsel = mAssistant->catalogSelection();
  slotAddPosition( catsel->currentSelectedKat(), 0 );
}

void KraftView::slotAddPosition( Katalog *kat, void *tmpl )
{
  int newpos = mPositionWidgetList.count()+1;
  kdDebug() << "Adding Position at position " << newpos << endl;

  TemplToPositionDialogBase *dia = 0;

  DocPosition *dp = new DocPosition();
  QSize s;

  bool newTemplate = false;
  if ( !tmpl ) {
    newTemplate = true;
  }

  if ( newTemplate ) {
    // New templates may only go to the standard template catalog, FIXME
    dia = new InsertTemplDialog( this );
    dia->setCatalogChapters( kat->getKatalogChapters() );
  } else {
    // it's not a new template
    if ( kat ) {
      // For empty template in plants dialog come up with standard dialog
      if ( kat->type() == TemplateCatalog ) {
        dia = new InsertTemplDialog( this );
        FloskelTemplate *ftmpl = static_cast<FloskelTemplate*>( tmpl );
        dp->setText( ftmpl->getText() );
        dp->setUnit( ftmpl->einheit() );
        dp->setUnitPrice( ftmpl->einheitsPreis() );

        s = KraftSettings::self()->templateToPosDialogSize();

      } else if ( kat->type() == MaterialCatalog ) {
        dia = new InsertTemplDialog( this );
        StockMaterial *mat = static_cast<StockMaterial*>( tmpl );
        dp->setText( mat->name() );
        dp->setUnit( mat->getUnit() );
        dp->setUnitPrice( mat->salesPrice() );
        s = KraftSettings::self()->templateToPosDialogSize();

      } else if ( kat->type() == PlantCatalog ) {
        dia = new InsertPlantDialog( this );
        InsertPlantDialog *plantDia = static_cast<InsertPlantDialog*>( dia );
        BrunsRecord *bruns = static_cast<BrunsRecord*>( tmpl );
        plantDia->setSelectedPlant( bruns );
        s = KraftSettings::self()->plantTemplateToPosDialogSize();
      }
    }
  }

  if ( dia ) {
    if ( mRememberAmount > 0 ) {
      dp->setAmount( mRememberAmount );
    }

    dia->setDocPosition( dp, newTemplate );

    dia->setPositionList( currentPositionList(), newpos );

    dia->setInitialSize( s );

    if ( dia->exec() ) {
      *dp = dia->docPosition();

      // store the initial size of the template-to-doc-pos dialogs
      s = dia->size();

      if ( kat->type() == TemplateCatalog ) {
        KraftSettings::self()->setTemplateToPosDialogSize( s );

        // if it's a new position, create a catalog template in the incoming chapter
        if ( newTemplate ) {
          const QString chapter = dia->chapter();
          if ( !chapter.isEmpty() ) {
            FloskelTemplate *flos = new FloskelTemplate( -1, dp->text(),
                                                         dp->unit().id(),
                                                         kat->chapterID( chapter ),
                                                         1, /* CalcKind = Manual */
                                                         QDateTime::currentDateTime(),
                                                         QDateTime::currentDateTime() );
            flos->setManualPrice( dp->unitPrice() );
            mNewTemplates.append( flos );
          }
        }
      } else if ( kat->type() == MaterialCatalog ) {
        KraftSettings::self()->setTemplateToPosDialogSize( s );
        if ( newTemplate ) {

        }

      } else if ( kat->type() == PlantCatalog ) {
        KraftSettings::self()->setPlantTemplateToPosDialogSize( s );
        if ( newTemplate ) {

        }

      }
      KraftSettings::self()->writeConfig();

      newpos = dia->insertAfterPosition();

      mRememberAmount = dp->amount();

      kdDebug() << "New position is " << dp->position() << " as int: " << newpos << endl;
    } else {
      return;
    }
  }

  delete dia;

  PositionViewWidget *widget = createPositionViewWidget( dp, newpos );
  widget->slotModified();
  slotFocusPosition( widget, newpos );
  refreshPostCard();
}

DocPositionList KraftView::currentPositionList()
{
    DocPositionList list;
    list.setLocale( *( m_doc->locale() ) );
    PositionViewWidget *widget;
    for( widget = mPositionWidgetList.first(); widget; widget = mPositionWidgetList.next() ) {
      DocPositionBase *dpb = widget->position();
      if ( dpb ) {
        DocPosition *dp = new DocPosition( );
        dp->setDbId( dpb->dbId().toInt() );
        // dp->setPosition( dpb->position() );

        dp->setToDelete( widget->deleted() );

        dp->setText( widget->m_teFloskel->text() );

        QString h = widget->m_cbUnit->currentText();
        int eId = UnitManager::getUnitIDSingular( h );
        Einheit e = UnitManager::getUnit( eId );
        dp->setUnit( e );

        dp->setUnitPrice( widget->unitPrice() );

        double v = widget->m_sbAmount->value();
        dp->setAmount( v );

        PositionViewWidget::Kind k = widget->kind();

        if ( k != PositionViewWidget::Normal ) {
          Attribute a( DocPosition::Kind );
          a.setPersistant( true );
          a.setValue( widget->kindString() );
          dp->setAttribute( a );
        } else {
          dp->removeAttribute( DocPosition::Kind );
        }

        list.append( dp );
      } else {
        kdError() << "Fatal: Widget without position found!" << endl;
      }
    }
    return list;
}

void KraftView::slotShowCatalog( bool on )
{
  if ( on ) {
    mAssistant->slotShowCatalog();
  } else {
    mAssistant->setFullPreview( true, KraftDoc::Positions );
  }
}

void KraftView::slotModifiedPositions()
{
  kdDebug() << "Position Modified" << endl;
  mModified = true;
}

void KraftView::slotModifiedHeader()
{
  kdDebug() << "Modified the header!" << endl;
  mModified = true;

  QTimer::singleShot( 0, this, SLOT( refreshPostCard() ) );
}

void KraftView::slotModifiedFooter()
{
  kdDebug() << "Modified the footer!" << endl;
  mModified = true;

  QTimer::singleShot( 0, this, SLOT( refreshPostCard() ) );
}

QStringList KraftView::generateLetterHead( Addressee adr )
{
    QStringList s;

    KraftDB::StringMap m;
    m[ "%NAME"]       = adr.familyName();
    m[ "%GIVEN_NAME"] = adr.givenName();

    return KraftDB::self()->wordList( "salut", m );
}

KraftDoc *KraftView::getDocument() const
{
  return m_doc;
}

void KraftView::done( int r )
{
  kdDebug() << "View closed with ret value " << r << endl;
  KraftDoc *doc = getDocument();
  if( doc )
    doc->removeView( this );
  KDialogBase::done( r );
}

void KraftView::slotOk()
{
    kdDebug() << "Accept Slot hit!" << endl;

    KraftDoc *doc = getDocument();

    if( !doc ) {
      kdDebug() << "ERR: No document available in view, return!" << endl;
      return;
    }
    // transfer all values to the document
    doc->setDate( m_headerEdit->m_dateEdit->date() );
    doc->setAddressUid( mContactUid );
    doc->setAddress(  m_headerEdit->m_postAddressEdit->text() );
    doc->setDocType(  m_headerEdit->m_cbType->currentText() );
    doc->setPreText(  m_headerEdit->m_teEntry->text() );
    doc->setWhiteboard( m_headerEdit->m_whiteboardEdit->text() );
    doc->setSalut(    m_headerEdit->m_letterHead->currentText() );
    doc->setPostText( m_footerEdit->m_teSummary->text() );
    doc->setGoodbye(  m_footerEdit->m_cbGreeting->currentText() );

    doc->setPositionList( currentPositionList() );

    doc->saveDocument( );

    if ( doc->isNew() ) {
      // For new documents the user had to select a greeting and we make this
      // default for the future
      KraftSettings::self()->setGreeting( m_footerEdit->m_cbGreeting->currentText() );
      KraftSettings::self()->setSalut( m_headerEdit->m_letterHead->currentItem() );
    }

    KraftSettings::self()->setDocViewSplitter( mCSplit->sizes() );
    KraftSettings::self()->setDocViewSize( size() );
    KraftSettings::self()->setDocViewPosition( pos() );
    KraftSettings::self()->writeConfig();

    // Save newly created templates
    if ( mNewTemplates.count() > 0 ) {
      for ( CatalogTemplate *ct = mNewTemplates.first(); ct; ct = mNewTemplates.next() ) {
        ct->save();
      }
      mNewTemplates.clear();
      // reload the entire katalog
      Katalog *defaultKat = KatalogMan::self()->defaultTemplateCatalog();
      KatalogMan::self()->notifyKatalogChange( defaultKat , dbID() );
    }

    emit viewClosed( true, m_doc );
    KDialogBase::slotOk(  );
}

void KraftView::slotFocusPosition( PositionViewWidget *posWidget, int pos )
{
  kdDebug() << "Focussing on widget " << posWidget << " on pos " << pos << endl;
  if( posWidget && pos > 0) {
    int w = posWidget->height();
    m_positionScroll->ensureVisible( 0, (pos)*w, 0, w );
  } else {
    m_positionScroll->ensureVisible( 0, 0 );
  }
  if( posWidget ) {
    if( posWidget->m_teFloskel->text().isEmpty() ) {
      posWidget->m_teFloskel->setFocus();
    } else {
      posWidget->m_sbAmount->setFocus();
    }
  }
}

void KraftView::slotCancel()
{
  // We need to reread the document
  if ( mModified ) {
    if ( KMessageBox::warningYesNo( this, i18n( "The document was modified, do "
                                                "you really want to discard all changes?" ) )
          == KMessageBox::No  )
    {
      return;
    }
  }

  KraftDoc *doc = getDocument();
  if( doc && doc->isModified() ) {
    kdDebug() << "Document refetch from database" << endl;
    doc->reloadDocument();
  }

  mNewTemplates.clear();

  emit viewClosed( false, 0 );
  KDialogBase::slotCancel();
}

void KraftView::print(QPrinter * /* pPrinter */ )
{
    // create a archive document and start printing
    // ArchivedDoc *archDoc = doc->archive();

}

#include "kraftview.moc"
