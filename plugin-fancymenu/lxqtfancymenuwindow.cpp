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

#include <QMenu>
#include <QStandardPaths>
#include <QMimeData>
#include <XdgIcon>
#include <QFile>

#include <QApplication>
#include <QClipboard>

#include <QBoxLayout>

#include <QMessageBox>

#include <QProcess>

#include <QKeyEvent>
#include <QCoreApplication>

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
    connect(mSearchEdit, &QLineEdit::textEdited, this, &LXQtFancyMenuWindow::setSearchQuery);
    connect(mSearchEdit, &QLineEdit::returnPressed, this, &LXQtFancyMenuWindow::activateCurrentApp);

    mSettingsButton = new QToolButton;
    mSettingsButton->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop"))); //TODO: preferences-system?
    mSettingsButton->setText(tr("Settings"));
    mSettingsButton->setToolTip(mSettingsButton->text());
    connect(mSettingsButton, &QToolButton::clicked, this, &LXQtFancyMenuWindow::runSystemConfigDialog);

    mPowerButton = new QToolButton;
    mPowerButton->setIcon(QIcon::fromTheme(QStringLiteral("system-shutdown")));
    mPowerButton->setText(tr("Leave"));
    mPowerButton->setToolTip(mPowerButton->text());
    connect(mPowerButton, &QToolButton::clicked, this, &LXQtFancyMenuWindow::runPowerDialog);

    mAppView = new QListView;
    mAppView->setUniformItemSizes(true);
    mAppView->setSelectionMode(QListView::SingleSelection);
    mAppView->setDragEnabled(true);
    mAppView->setContextMenuPolicy(Qt::CustomContextMenu);

    mCategoryView = new QListView;
    mCategoryView->setUniformItemSizes(true);
    mCategoryView->setSelectionMode(QListView::SingleSelection);

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

    connect(mAppView, &QListView::activated, this, &LXQtFancyMenuWindow::activateAppAtIndex);
    connect(mAppView, &QListView::customContextMenuRequested, this, &LXQtFancyMenuWindow::onAppViewCustomMenu);
    connect(mCategoryView, &QListView::activated, this, &LXQtFancyMenuWindow::activateCategory);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(mSettingsButton);
    buttonLayout->addWidget(mPowerButton);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addWidget(mSearchEdit);

    // Use 3:2 stretch factors so app view is slightly wider than category view
    QHBoxLayout *viewLayout = new QHBoxLayout;
    viewLayout->addWidget(mAppView, 3);
    viewLayout->addWidget(mCategoryView, 2);
    mainLayout->addLayout(viewLayout);

    setMinimumHeight(500);

    // Ensure all key presses go to search box
    setFocusProxy(mSearchEdit);
    mAppView->setFocusProxy(mSearchEdit);
    mCategoryView->setFocusProxy(mSearchEdit);

    // Filter navigation keys
    mSearchEdit->installEventFilter(this);
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

    setCurrentCategory(LXQtFancyMenuAppMap::FavoritesCategory);

    return true;
}

void LXQtFancyMenuWindow::activateCategory(const QModelIndex &idx)
{
    setCurrentCategory(idx.row());
}

void LXQtFancyMenuWindow::activateAppAtIndex(const QModelIndex &idx)
{
    if(!idx.isValid())
        return;

    auto *app = mAppModel->getAppAt(idx.row());
    if(!app)
        return;

    app->desktopFileCache.startDetached();
    hide();
}

void LXQtFancyMenuWindow::activateCurrentApp()
{
    activateAppAtIndex(mAppView->currentIndex());
}

void LXQtFancyMenuWindow::runPowerDialog()
{
    runCommandHelper(QLatin1String("lxqt-leave"));
}

void LXQtFancyMenuWindow::runSystemConfigDialog()
{
    runCommandHelper(QLatin1String("lxqt-config"));
}

void LXQtFancyMenuWindow::onAppViewCustomMenu(const QPoint& p)
{
    QModelIndex idx = mAppView->indexAt(p);
    auto item = mAppModel->getAppAt(idx.row());
    if(!item)
        return;

    XdgDesktopFile df = item->desktopFileCache;
    QString file = df.fileName();

    QMenu menu;
    QAction *a;

    if (df.actions().count() > 0 && df.type() == XdgDesktopFile::Type::ApplicationType)
    {
        for (int i = 0; i < df.actions().count(); ++i)
        {
            QString actionString(df.actions().at(i));
            a = menu.addAction(df.actionIcon(actionString), df.actionName(actionString));
            connect(a, &QAction::triggered, this, [this, df, actionString] {
                df.actionActivate(actionString, QStringList());
                hide();
            });
        }
        menu.addSeparator();
    }

    a = menu.addAction(XdgIcon::fromTheme(QLatin1String("desktop")), tr("Add to desktop"));
    connect(a, &QAction::triggered, [file] {
        QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString desktopFile = desktop + QStringLiteral("/") + file.section(QStringLiteral("/"), -1);
        if (QFile::exists(desktopFile))
        {
            QMessageBox::StandardButton btn =
                QMessageBox::question(nullptr,
                                      tr("Question"),
                                      tr("A file with the same name already exists.\nDo you want to overwrite it?"));
            if (btn == QMessageBox::No)
                return;
            if (!QFile::remove(desktopFile))
            {
                QMessageBox::warning(nullptr,
                                     tr("Warning"),
                                     tr("The file cannot be overwritten."));
                return;
            }
        }
        QFile::copy(file, desktopFile);
    });
    a = menu.addAction(XdgIcon::fromTheme(QLatin1String("edit-copy")), tr("Copy"));
    connect(a, &QAction::triggered, this, [file] {
        QClipboard* clipboard = QApplication::clipboard();
        QMimeData* data = new QMimeData();
        data->setUrls({QUrl::fromLocalFile(file)});
        clipboard->setMimeData(data);
    });

    QPoint globalPos = mAppView->mapToGlobal(p);
    menu.exec(globalPos);
}

void LXQtFancyMenuWindow::setCurrentCategory(int cat)
{
    QModelIndex idx = mCategoryModel->index(cat, 0);
    mCategoryView->setCurrentIndex(idx);
    mCategoryView->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
    mAppModel->setCurrentCategory(cat);
    mAppModel->endSearch();
}

bool LXQtFancyMenuWindow::eventFilter(QObject *watched, QEvent *e)
{
    if(watched == mSearchEdit && e->type() == QEvent::KeyPress)
    {
        QKeyEvent *ev = static_cast<QKeyEvent *>(e);
        if(ev->key() == Qt::Key_Up || ev->key() == Qt::Key_Down)
        {
            // Use Up/Down arrows to navigate app view
            QCoreApplication::sendEvent(mAppView, ev);
            return true;
        }
    }

    return QWidget::eventFilter(watched, e);
}

void LXQtFancyMenuWindow::setSearchQuery(const QString &text)
{
    QSignalBlocker blk(mSearchEdit);
    mSearchEdit->setText(text);

    if(text.isEmpty())
    {
        mAppModel->endSearch();
        return;
    }

    setCurrentCategory(LXQtFancyMenuAppMap::AllAppsCategory);

    auto apps = mAppMap->getMatchingApps(text);
    mAppModel->showSearchResults(apps);
}

void LXQtFancyMenuWindow::runCommandHelper(const QString &cmd)
{
    if(QProcess::startDetached(cmd, QStringList()))
    {
        hide();
    }
    else
    {
        QMessageBox::warning(this, tr("No Executable"),
                             tr("Cannot find <b>%1</b> executable.").arg(cmd));
    }
}
