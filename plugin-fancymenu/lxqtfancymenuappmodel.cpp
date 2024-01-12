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


#include "lxqtfancymenuappmodel.h"
#include "lxqtfancymenuappmap.h"

LXQtFancyMenuAppModel::LXQtFancyMenuAppModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LXQtFancyMenuAppModel::rowCount(const QModelIndex &p) const
{
    if(!mAppMap || p.isValid() || mCurrentCategory < 0 || mCurrentCategory >= mAppMap->getCategoriesCount())
        return 0;

    if(mCurrentCategory == LXQtFancyMenuAppMap::AllAppsCategory)
        return mAppMap->getTotalAppCount(); //Special "All Applications" category
    return mAppMap->getCategoryAt(mCurrentCategory).apps.size();
}

QVariant LXQtFancyMenuAppModel::data(const QModelIndex &idx, int role) const
{
    if(!idx.isValid())
        return QVariant();

    const LXQtFancyMenuAppMap::AppItem* item = getAppAt(idx.row());
    if(!item)
        return QVariant();

    switch (role)
    {
    case Qt::DisplayRole:
        return item->title;
    case Qt::EditRole:
        return item->desktopFile;
    case Qt::DecorationRole:
        return item->icon;
    case Qt::ToolTipRole:
        return item->comment;
    default:
        break;
    }

    return QVariant();
}

void LXQtFancyMenuAppModel::reloadAppMap(bool end)
{
    if(!end)
        beginResetModel();
    else
        endResetModel();
}

void LXQtFancyMenuAppModel::setCurrentCategory(int category)
{
    beginResetModel();
    mCurrentCategory = category;
    endResetModel();
}

LXQtFancyMenuAppMap *LXQtFancyMenuAppModel::appMap() const
{
    return mAppMap;
}

void LXQtFancyMenuAppModel::setAppMap(LXQtFancyMenuAppMap *newAppMap)
{
    mAppMap = newAppMap;
}

const LXQtFancyMenuAppItem *LXQtFancyMenuAppModel::getAppAt(int idx) const
{
    if(!mAppMap || idx < 0 || mCurrentCategory < 0 || mCurrentCategory >= mAppMap->getCategoriesCount())
        return nullptr;

    if(mCurrentCategory == LXQtFancyMenuAppMap::AllAppsCategory)
        return mAppMap->getAppAt(idx); //Special "All Applications" category

    return mAppMap->getCategoryAt(mCurrentCategory).apps.value(idx, nullptr);
}
