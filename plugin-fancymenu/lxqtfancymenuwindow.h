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


#ifndef LXQTFANCYMENUWINDOW_H
#define LXQTFANCYMENUWINDOW_H

#include <QWidget>

class QLineEdit;
class QToolButton;
class QListView;
class QModelIndex;

class XdgMenu;

class LXQtFancyMenuAppMap;
class LXQtFancyMenuAppModel;
class LXQtFancyMenuCategoriesModel;

class LXQtFancyMenuWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LXQtFancyMenuWindow(QWidget *parent = nullptr);
    ~LXQtFancyMenuWindow();

    virtual QSize sizeHint() const override;

    bool rebuildMenu(const XdgMenu &menu);

    void setCurrentCategory(int cat);

    bool eventFilter(QObject *watched, QEvent *e) override;

public slots:
    void setSearchQuery(const QString& text);

protected:
    void keyPressEvent(QKeyEvent *e);

private slots:
    void activateCategory(const QModelIndex& idx);
    void activateAppAtIndex(const QModelIndex& idx);
    void activateCurrentApp();

    void runPowerDialog();
    void runSystemConfigDialog();

    void onAppViewCustomMenu(const QPoint &p);

private:
    void runCommandHelper(const QString& cmd);

private:
    QToolButton *mSettingsButton;
    QToolButton *mPowerButton;
    QLineEdit *mSearchEdit;
    QListView *mAppView;
    QListView *mCategoryView;

    LXQtFancyMenuAppMap *mAppMap;
    LXQtFancyMenuAppModel *mAppModel;
    LXQtFancyMenuCategoriesModel *mCategoryModel;
};

#endif // LXQTFANCYMENUWINDOW_H
