//===========================================
//  Lumina Desktop source code
//  Copyright (c) 2017, Ken Moore
//  Available under the 3-clause BSD license
//  See the LICENSE file for full details
//===========================================

#include "BookmarkMenu.h"
#include "ui_BookmarkMenu.h"
#include <QPushButton>
#include <QSplitter>

BookmarkMenu::BookmarkMenu(Renderer *Backend, QWidget *parent)
    : QWidget(parent), ui(new Ui::BookmarkMenu()), BACKEND(Backend) {
  ui->setupUi(this);
  connect(ui->closeButton, &QPushButton::clicked, parent, [=]() {
    static_cast<QSplitter *>(parent)->setSizes(QList<int>()
                                               << 0 << this->width());
  });
  connect(ui->closeButton, &QPushButton::clicked, parent, [=]() {});
  connect(ui->bookmarks, &QTreeWidget::itemClicked, this,
          [=](QTreeWidgetItem *item) {
            Backend->handleLink(parent, item->data(1, Qt::UserRole).toString());
          });
}

void BookmarkMenu::loadBookmarks() {
  QTreeWidgetItem *item = nullptr;
  QTreeWidgetItem *parent = nullptr;
  QList<Bookmark *> bookmarks = BACKEND->getBookmarks();

  if (ui->bookmarks->topLevelItemCount() != 0) {
    ui->bookmarks->clear();
  }

  // Modfiy for more than 2 levels
  if (bookmarks.empty()) {
    item = new QTreeWidgetItem(ui->bookmarks);
    item->setText(0, "No Bookmarks");
    item->setData(1, Qt::UserRole, "");
    item->setIcon(0, QIcon::fromTheme("bookmark-remove"));
  } else {
    foreach (Bookmark *bm, bookmarks) {
      if (bm->level == 0) {
        item = new QTreeWidgetItem(ui->bookmarks);
        parent = item;
      } else {
        item = new QTreeWidgetItem(parent);
      }
      item->setText(0, bm->title);
      item->setData(1, Qt::UserRole, bm->link);
      if (!bm->link.isEmpty()) {
        item->setIcon(0, QIcon::fromTheme("bookmark-new"));
      } else {
        item->setIcon(0, QIcon::fromTheme("bookmark-remove"));
}
    }
  }
}
