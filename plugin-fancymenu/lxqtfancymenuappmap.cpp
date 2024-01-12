/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXQt - a lightweight, Qt based, desktop toolset
 * https://lxqt.org
 *
 * Copyright: 2023 LXQt team
 * Authors:
 *  Filippo Gentile <filippogentile@disroot.org>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */


#include "lxqtfancymenuappmap.h"

#include <XdgMenu>

#include <QCoreApplication>

class LXQtFancyMenuAppMapStrings
{
    Q_DECLARE_TR_FUNCTIONS(LXQtFancyMenuAppMapStrings)
};


LXQtFancyMenuAppMap::LXQtFancyMenuAppMap()
{
    mCachedIndex = -1;
    mCachedIterator = mAppSortedByName.constEnd();
}

LXQtFancyMenuAppMap::~LXQtFancyMenuAppMap()
{
    clear();
}

void LXQtFancyMenuAppMap::clear()
{
    mCategories.clear();
    mAppSortedByName.clear();
    qDeleteAll(mAppSortedByDesktopFile);
    mAppSortedByDesktopFile.clear();

    mCachedIndex = -1;
    mCachedIterator = mAppSortedByName.constEnd();
}

bool LXQtFancyMenuAppMap::rebuildModel(const XdgMenu &menu)
{
    clear();

    //Add Favorites category
    Category favorites;
    favorites.menuTitle = LXQtFancyMenuAppMapStrings::tr("Favorites");
    favorites.icon = QIcon::fromTheme(QLatin1String("bookmarks"));
    mCategories.append(favorites);

    //Add All Apps category
    Category allAppsCategory;
    allAppsCategory.menuTitle = LXQtFancyMenuAppMapStrings::tr("All Applications");
    allAppsCategory.icon = QIcon::fromTheme(QLatin1String("folder"));
    mCategories.append(allAppsCategory);

    //TODO: add separator

    QDomElement rootMenu = menu.xml().documentElement();
    parseMenu(rootMenu, QString());
    return true;
}

LXQtFancyMenuAppMap::AppItem *LXQtFancyMenuAppMap::getAppAt(int index)
{
    if(index < 0 || index >= getTotalAppCount())
        return nullptr;

    if(mCachedIndex != -1)
    {
        if(index == mCachedIndex + 1)
        {
            //Fast case, go to next row
            mCachedIndex++;
            mCachedIterator++;
        }

        if(index == mCachedIndex)
            return *mCachedIterator;

        int dist1 = qAbs(mCachedIndex - index);
        if(dist1 < index)
        {
            std::advance(mCachedIterator, index - mCachedIndex);
            mCachedIndex = index;
            return *mCachedIterator;
        }
    }

    // Recalculate cached iterator
    mCachedIterator = mAppSortedByName.constBegin();
    std::advance(mCachedIterator, index);
    mCachedIndex = index;
    return *mCachedIterator;
}

QVector<const LXQtFancyMenuAppMap::AppItem *> LXQtFancyMenuAppMap::getMatchingApps(const QString &query) const
{
    QVector<const AppItem *> byName;
    QVector<const AppItem *> byKeyword;

    //TODO: implement some kind of score to get better matches on top

    for(const AppItem *app : qAsConst(mAppSortedByName))
    {
        if(app->title.contains(query))
        {
            byName.append(app);
            continue;
        }

        if(app->comment.contains(query))
        {
            byKeyword.append(app);
            continue;
        }

        for(const QString& key : app->keywords)
        {
            if(key.startsWith(query))
            {
                byKeyword.append(app);
                break;
            }
        }
    }

    // Give priority to title matches
    byName += byKeyword;

    return byName;
}

void LXQtFancyMenuAppMap::parseMenu(const QDomElement &menu, const QString& topLevelCategory)
{
    QDomElement e = menu.firstChildElement();
    while(!e.isNull())
    {
        if(e.tagName() == QLatin1String("Menu"))
        {
            if(topLevelCategory.isEmpty())
            {
                //This is a top level menu
                Category item;
                item.menuName = e.attribute(QLatin1String("name"));
                item.menuTitle = e.attribute(QLatin1Literal("title"), item.menuName);
                QString iconName = e.attribute(QLatin1String("icon"));
                item.icon = QIcon::fromTheme(iconName);
                mCategories.append(item);

                //Merge sub menu to parent
                parseMenu(e, item.menuName);
            }
            else
            {
                //Merge sub menu to parent
                parseMenu(e, topLevelCategory);
            }
        }
        else if(!topLevelCategory.isEmpty() && e.tagName() == QLatin1String("AppLink"))
        {
            parseAppLink(e, topLevelCategory);
        }

        e = e.nextSiblingElement();
    }
}

void LXQtFancyMenuAppMap::parseAppLink(const QDomElement &app, const QString& topLevelCategory)
{
    QString desktopFile = app.attribute(QLatin1String("desktopFile"));

    // Check if already added
    AppItem *item = mAppSortedByDesktopFile.value(desktopFile, nullptr);
    if(!item)
    {
        // Add new app
        item = loadAppItem(desktopFile);
        if(!item)
            return; // Invalid app

        mAppSortedByDesktopFile.insert(item->desktopFile, item);
        mAppSortedByName.insert(item->title, item);
    }

    // Now add app to category
    for(Category &category : mCategories)
    {
        if(category.menuName == topLevelCategory)
        {
            category.apps.append(item);
            break;
        }
    }
}

LXQtFancyMenuAppMap::AppItem *LXQtFancyMenuAppMap::loadAppItem(const QString &desktopFile)
{
    XdgDesktopFile f;
    if(!f.load(desktopFile))
        return nullptr; // Invalid App

    AppItem *item = new AppItem;
    item->desktopFile = desktopFile;
    item->title = f.name();
    item->comment = f.comment();
    if(item->comment.isEmpty())
        item->comment = f.localizedValue(QLatin1String("GenericName")).toString();
    item->icon = f.icon();
    item->desktopFileCache = f;

    item->keywords << f.localizedValue(QLatin1String("Keywords")).toString().toLower().split(QLatin1Char(';'));
    item->keywords.append(item->title.toLower().split(QLatin1Char(' ')));
    item->keywords.append(item->comment.toLower().split(QLatin1Char(' ')));
    return item;
}
