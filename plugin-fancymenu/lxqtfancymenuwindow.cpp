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


#include "lxqtfancymenuwindow.h"

#include "lxqtfancymenuappmap.h"
#include "lxqtfancymenuappmodel.h"
#include "lxqtfancymenucategoriesmodel.h"

#include <QLineEdit>
#include <QToolButton>
#include <QListView>

#include <QBoxLayout>

#include <QProxyStyle>

namespace
{
class SingleActivateStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;
    int styleHint(StyleHint hint, const QStyleOption * option = nullptr, const QWidget * widget = nullptr, QStyleHintReturn * returnData = nullptr) const override
    {
        if(hint == QStyle::SH_ItemView_ActivateItemOnSingleClick)
            return 1;
        return QProxyStyle::styleHint(hint, option, widget, returnData);

    }
};
}

LXQtFancyMenuWindow::LXQtFancyMenuWindow(QWidget *parent)
    : QWidget{parent, Qt::Popup}
{
    SingleActivateStyle *s = new SingleActivateStyle(style());
    s->setParent(this);
    setStyle(s);

    mSearchEdit = new QLineEdit;
    mSearchEdit->setPlaceholderText(tr("Search..."));
    mSearchEdit->setClearButtonEnabled(true);

    mPowerButton = new QToolButton;
    mPowerButton->setIcon(QIcon::fromTheme(QStringLiteral("system-shutdown")));
    mPowerButton->setText(tr("Leave"));
    mPowerButton->setToolTip(mPowerButton->text());

    mAppView = new QListView;
    mAppView->setUniformItemSizes(true);

    mCategoryView = new QListView;
    mCategoryView->setUniformItemSizes(true);

    // Meld category view with whole popup window
    // So remove the frame and set same background as the window
    mCategoryView->setFrameShape(QFrame::NoFrame);
    mCategoryView->viewport()->setBackgroundRole(QPalette::Window);

    mAppMap = new LXQtFancyMenuAppMap;

    mAppModel = new LXQtFancyMenuAppModel(this);
    mAppModel->setAppMap(mAppMap);
    mAppView->setModel(mAppModel);

    mCategoryModel = new LXQtFancyMenuCategoriesModel(this);
    mCategoryModel->setAppMap(mAppMap);
    mCategoryView->setModel(mCategoryModel);

    connect(mCategoryView, &QListView::activated, this, &LXQtFancyMenuWindow::activateCategory);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(mPowerButton);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addWidget(mSearchEdit);

    QHBoxLayout *viewLayout = new QHBoxLayout;
    viewLayout->addWidget(mAppView);
    viewLayout->addWidget(mCategoryView);
    mainLayout->addLayout(viewLayout);

    setMinimumHeight(500);
}

LXQtFancyMenuWindow::~LXQtFancyMenuWindow()
{
    mAppModel->setAppMap(nullptr);
    mCategoryModel->setAppMap(nullptr);
    delete mAppMap;
    mAppMap = nullptr;
}

QSize LXQtFancyMenuWindow::sizeHint() const
{
    return QSize(450, 550);
}

bool LXQtFancyMenuWindow::rebuildMenu(const XdgMenu &menu)
{
    mAppModel->reloadAppMap(false);
    mCategoryModel->reloadAppMap(false);
    mAppMap->rebuildModel(menu);
    mAppModel->reloadAppMap(true);
    mCategoryModel->reloadAppMap(true);
    return true;
}

void LXQtFancyMenuWindow::activateCategory(const QModelIndex &idx)
{
    mAppModel->setCurrentCategory(idx.row());
}
