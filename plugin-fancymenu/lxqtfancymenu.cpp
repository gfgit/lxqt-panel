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


#include "lxqtfancymenu.h"
#include "lxqtfancymenuconfiguration.h"
#include "../panel/lxqtpanel.h"
#include <QTimer>
#include <QMessageBox>
#include <QEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <lxqt-globalkeys.h>
#include <QApplication>
#include <QMetaEnum>
#include <QStringBuilder>

#include <XdgMenuWidget>
#include <XdgIcon>

#include <QStandardPaths>
#include <QClipboard>
#include <QMimeData>
#include <XdgAction>

#define DEFAULT_SHORTCUT "Alt+F1"

LXQtFancyMenu::LXQtFancyMenu(const ILXQtPanelPluginStartupInfo &startupInfo):
    QObject(),
    ILXQtPanelPlugin(startupInfo),
    mShortcut(nullptr),
    mFilterClear(false)
{
    mDelayedPopup.setSingleShot(true);
    mDelayedPopup.setInterval(200);
    connect(&mDelayedPopup, &QTimer::timeout, this, &LXQtFancyMenu::showHideMenu);
    mHideTimer.setSingleShot(true);
    mHideTimer.setInterval(250);

    mButton.setAutoRaise(true);
    mButton.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    //Notes:
    //1. installing event filter to parent widget to avoid infinite loop
    //   (while setting icon we also need to set the style)
    //2. delaying of installEventFilter because in c-tor mButton has no parent widget
    //   (parent is assigned in panel's logic after widget() call)
    QTimer::singleShot(0, mButton.parentWidget(), [this] {
        Q_ASSERT(mButton.parentWidget());
        mButton.parentWidget()->installEventFilter(this);
    });

    connect(&mButton, &QToolButton::clicked, this, &LXQtFancyMenu::showHideMenu);

    QTimer::singleShot(0, this, [this] {
        settingsChanged();
    });

    mShortcut = GlobalKeyShortcut::Client::instance()->addAction(QString{}, QStringLiteral("/panel/%1/show_hide").arg(settings()->group()), LXQtFancyMenu::tr("Show/hide main menu"), this);
    if (mShortcut)
    {
        connect(mShortcut, &GlobalKeyShortcut::Action::shortcutChanged, this, [this](const QString &, const QString & shortcut) {
                mShortcutSeq = shortcut;
        });
        connect(mShortcut, &GlobalKeyShortcut::Action::registrationFinished, this, [this] {
            if (mShortcut->shortcut().isEmpty())
                mShortcut->changeShortcut(QStringLiteral(DEFAULT_SHORTCUT));
            else
                mShortcutSeq = mShortcut->shortcut();
        });
        connect(mShortcut, &GlobalKeyShortcut::Action::activated, this, [this] {
            if (!mHideTimer.isActive())
                // Delay this a little -- if we don't do this, search field
                // won't be able to capture focus
                // See <https://github.com/lxqt/lxqt-panel/pull/131> and
                // <https://github.com/lxqt/lxqt-panel/pull/312>
                mDelayedPopup.start();
        });
    }
}


/************************************************

 ************************************************/
LXQtFancyMenu::~LXQtFancyMenu()
{
    mButton.parentWidget()->removeEventFilter(this);
}


/************************************************

 ************************************************/
void LXQtFancyMenu::showHideMenu()
{

}

/************************************************

 ************************************************/
void LXQtFancyMenu::showMenu()
{

}

/************************************************

 ************************************************/
void LXQtFancyMenu::settingsChanged()
{
    setButtonIcon();
    if (settings()->value(QStringLiteral("showText"), false).toBool())
    {
        mButton.setText(settings()->value(QStringLiteral("text"), QStringLiteral("Start")).toString());
        mButton.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    else
    {
        mButton.setText(QLatin1String(""));
        mButton.setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    mLogDir = settings()->value(QStringLiteral("log_dir"), QString()).toString();

    QString menu_file = settings()->value(QStringLiteral("menu_file"), QString()).toString();
    if (menu_file.isEmpty())
        menu_file = XdgMenu::getMenuFileName();

    if (mMenuFile != menu_file)
    {
        mMenuFile = menu_file;
        mXdgMenu.setEnvironments(QStringList() << QStringLiteral("X-LXQT") << QStringLiteral("LXQt"));
        mXdgMenu.setLogDir(mLogDir);

        bool res = mXdgMenu.read(mMenuFile);
        connect(&mXdgMenu, &XdgMenu::changed, this, &LXQtFancyMenu::buildMenu);
        if (res)
        {
            QTimer::singleShot(1000, this, &LXQtFancyMenu::buildMenu);
        }
        else
        {
            QMessageBox::warning(nullptr, QStringLiteral("Parse error"), mXdgMenu.errorString());
            return;
        }
    }

    setMenuFontSize();

    //clear the search to not leaving the menu in wrong state
    mFilterClear = settings()->value(QStringLiteral("filterClear"), false).toBool();

    realign();
}

/************************************************

 ************************************************/
void LXQtFancyMenu::buildMenu()
{
    setMenuFontSize();
}

/************************************************

 ************************************************/
void LXQtFancyMenu::setMenuFontSize()
{

}

/************************************************

 ************************************************/
void LXQtFancyMenu::setButtonIcon()
{
    if (settings()->value(QStringLiteral("ownIcon"), false).toBool())
    {
        mButton.setStyleSheet(QStringLiteral("#FancyMenu { qproperty-icon: url(%1); }")
                .arg(settings()->value(QLatin1String("icon"), QLatin1String(LXQT_GRAPHICS_DIR"/helix.svg")).toString()));
    } else
    {
        mButton.setStyleSheet(QString());
    }
}

/************************************************

 ************************************************/
QDialog *LXQtFancyMenu::configureDialog()
{
    return new LXQtFancyMenuConfiguration(settings(), mShortcut, QStringLiteral(DEFAULT_SHORTCUT));
}

/************************************************

 ************************************************/
bool LXQtFancyMenu::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == mButton.parentWidget())
    {
        // the application is given a new QStyle
        if(event->type() == QEvent::StyleChange)
        {
            setMenuFontSize();
            setButtonIcon();
        }
    }
    return false;
}

#undef DEFAULT_SHORTCUT
