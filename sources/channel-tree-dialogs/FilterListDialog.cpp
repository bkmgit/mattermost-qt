/**
 * Copyright 2021, 2022 Lyubomir Filipov
 *
 * This file is part of Mattermost-QT.
 *
 * Mattermost-QT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mattermost-QT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Mattermost-QT. if not, see https://www.gnu.org/licenses/.
 */

#include "FilterListDialog.h"
#include "ui_FilterListDialog.h"

namespace Mattermost {

FilterListDialog::FilterListDialog (QWidget* parent)
:QDialog(parent)
,ui(new Ui::FilterListDialog)
{
    ui->setupUi(this);

    connect (ui->treeWidget, &QTreeWidget::customContextMenuRequested, this, &FilterListDialog::showContextMenu);
    connect (ui->filterLineEdit, &QLineEdit::textEdited, this, &FilterListDialog::applyFilter);
}

FilterListDialog::~FilterListDialog()
{
    delete ui;
}

void FilterListDialog::applyFilter (const QString& text)
{
	uint32_t usersCount = 0;

	for (int row = 0; row < ui->treeWidget->topLevelItemCount(); ++row) {
		QTreeWidgetItem* item = ui->treeWidget->topLevelItem (row);

		if (item->text(0).contains(text, Qt::CaseInsensitive)) {
			item->setHidden(false);
			++usersCount;
		} else {
			item->setHidden(true);
		}
	}

	ui->usersCountLabel->setText(QString::number(usersCount) + " users");
}

} /* namespace Mattermost */
