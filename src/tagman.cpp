/***************************************************************************
             TagTemplateManager - Manage the tag templates
                             -------------------
    begin                : June 2008
    copyright            : (C) 2008 by Klaas Freitag
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
#include <QtGui>
#include <QSqlQuery>

// include files for KDE
#include <klocale.h>
#include <kdebug.h>
#include <k3staticdeleter.h>

#include "tagman.h"
#include "kraftdb.h"

/*
 * ********** Tag Template  **********
 */

TagTemplate::TagTemplate()
{

}

TagTemplate::TagTemplate( const dbID& id, const QString& name, const QString& desc, const QString& col )
  : mId( id ), mName( name ), mDesc( desc ), mColor( col )
{

}

QColorGroup TagTemplate::colorGroup() const
{
  QColorGroup cg;
  cg.setColor( QColorGroup::Light, mColor.light() );
  cg.setColor( QColorGroup::Dark,  mColor.dark() );
  cg.setColor( QColorGroup::Mid,   mColor );
  return cg;
}

bool TagTemplate::operator!= ( const TagTemplate& tt ) const
{
  return !( mName == tt.mName && mDesc == tt.mDesc && mColor == tt.mColor );
}

/*
 * ********** Tag Template Manager **********
 */
static K3StaticDeleter<TagTemplateMan> selfDeleter;
TagTemplateMan* TagTemplateMan::mSelf = 0;

TagTemplateMan *TagTemplateMan::self()
{
  if ( !mSelf ) {
    selfDeleter.setObject( mSelf, new TagTemplateMan() );
  }
  return mSelf;
}

TagTemplateMan::TagTemplateMan( )
{
    load();
}

QStringList TagTemplateMan::allTagTemplates()
{
  QStringList list;

  TagTemplateValueVector::iterator it;
  for( it = mTagTmpl.begin(); it != mTagTmpl.end(); ++it )
  {
    QString n = (*it).name();
    if( !n.isEmpty())
      list << n;
  }
  return list;
}

TagTemplate TagTemplateMan::getTagTemplate( const QString& name )
{
    TagTemplateValueVector::iterator it;
    for( it = mTagTmpl.begin(); it != mTagTmpl.end(); ++it )
    {
        if( (*it).name() == name ) return (*it);
    }
    return TagTemplate();
}

TagTemplate TagTemplateMan::getTagTemplateFromId( const QString& id )
{
  TagTemplateValueVector::iterator it;
  for( it = mTagTmpl.begin(); it != mTagTmpl.end(); ++it )
  {
    if( (*it).dbId().toString() == id ) return (*it);
  }
  return TagTemplate();
}


TagTemplateMan::~TagTemplateMan( )
{

}

bool TagTemplateMan::writeTemplate( const TagTemplate& tt )
{
  bool ret = true;
  int cnt = 0;

  if ( tt.dbId().isOk() ) {
    QSqlQuery q;
    q.prepare( "UPDATE tagTemplates SET name=:name, description=:desc, color=:col "
               "WHERE tagTmplID=:id" );
    q.bindValue( ":name", tt.name() );
    q.bindValue( ":desc", tt.description() );
    q.bindValue( ":col",  tt.color().name() );
    q.bindValue( ":id",   tt.dbId().toString() );
    q.exec();
    cnt = q.numRowsAffected();
  }

  if ( cnt == -1 ) {
    kError() << "DB does not know the number of affected rows, poor!" << endl;
    ret = false;
  } else if ( cnt == 0 ) {
    kDebug() << "need to insert the tag template into db" << endl;
    QSqlQuery qi;
    qi.prepare( "INSERT INTO tagTemplates (name, description, color) VALUES "
                  "( :name, :desc, :col )" );
    qi.bindValue( ":name", tt.name() );
    qi.bindValue( ":desc", tt.description() );
    qi.bindValue( ":col",  tt.color().name() );
    qi.exec();
  }
  if ( ret ) {
    load();
  }
  return ret;
}

void TagTemplateMan::deleteTemplate( const dbID& id )
{
  if ( id.isOk() ) {
    QSqlQuery q;
    q.prepare( "DELETE FROM tagTemplates WHERE tagTmplID=:id" );
    q.bindValue( ":id", id.toString() );
    q.exec();
    load();
  }
}

void TagTemplateMan::load()
{
  mTagTmpl.clear();

  /* read tag templates from db */
  QSqlQuery q1( "SELECT tagTmplID, name, description, color FROM tagTemplates ORDER BY sortKey" );
  while( q1.next()) {
    dbID id( q1.value(0).toInt() );
    // resize if index is to big.
    TagTemplate tt ( id, q1.value(1).toString(), q1.value(2).toString(),
                     q1.value(3).toString() );
    mTagTmpl.append( tt );
  }
}


/* END */

