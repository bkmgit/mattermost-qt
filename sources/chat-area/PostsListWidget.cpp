/**
 * @file PostsListWidget.cpp
 * @brief
 * @author Lyubomir Filipov
 * @date Dec 27, 2021
 */

#include <QScrollBar>
#include <QDebug>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QResizeEvent>
#include "PostSeparatorWidget.h"
#include "backend/Backend.h"
#include "backend/types/BackendPost.h"
#include "PostWidget.h"
#include "PostsListWidget.h"
#include "PostWidget.h"

namespace Mattermost {

PostsListWidget::PostsListWidget (QWidget* parent)
:QListWidget (parent)
,backend (nullptr)
,newMessagesSeparator (nullptr)
{
	removeNewMessagesSeparatorTimer.setSingleShot (true);
	connect (&removeNewMessagesSeparatorTimer, &QTimer::timeout, this, &PostsListWidget::removeNewMessagesSeparator);

	connect (this, &QListWidget::customContextMenuRequested, this, &PostsListWidget::showContextMenu);
}

void PostsListWidget::insertPost (int position, PostWidget* postWidget)
{
	QListWidgetItem* newItem = new QListWidgetItem();
	insertItem (position, newItem);
	setItemWidget (newItem, postWidget);
	verticalScrollBar()->setSingleStep (10);
}

void PostsListWidget::insertPost (PostWidget* postWidget)
{
	QListWidgetItem* newItem = new QListWidgetItem();
	addItem (newItem);
	setItemWidget (newItem, postWidget);
	verticalScrollBar()->setSingleStep (10);
}

int PostsListWidget::findPostByIndex (const QString& postId, int startIndex)
{
	if (postId.isEmpty()) {
		return -1;
	}

	while (startIndex < count()) {

		PostWidget* message = static_cast <PostWidget*> (itemWidget (item (startIndex)));

		if (message->post.id == postId) {
			return startIndex;
		}

		++startIndex;
	}

	qDebug() << "Post with id " << postId << " not found";
	return -1;
}

PostWidget* PostsListWidget::findPost (const QString& postId)
{
	if (postId.isEmpty()) {
		return nullptr;
	}

	int startIndex = 0;

	while (startIndex < count()) {

		PostWidget* message = static_cast <PostWidget*> (itemWidget (item (startIndex)));

		if (message->post.id == postId) {
			return message;
		}

		++startIndex;
	}

	return nullptr;
}

void PostsListWidget::scrollToUnreadPostsOrBottom ()
{
	if (newMessagesSeparator) {
		scrollToItem (newMessagesSeparator, QAbstractItemView::PositionAtCenter);
	} else {
		scrollToBottom ();
	}
}

void PostsListWidget::addNewMessagesSeparator ()
{
	if (newMessagesSeparator) {
		return;
	}

	PostSeparatorWidget* separator = new PostSeparatorWidget ("New messages");
	newMessagesSeparator = new QListWidgetItem();
	addItem (newMessagesSeparator);
	setItemWidget (newMessagesSeparator, separator);
}

void PostsListWidget::removeNewMessagesSeparator ()
{
	if (!newMessagesSeparator) {
		return;
	}

	delete newMessagesSeparator;
	newMessagesSeparator = nullptr;
}

void PostsListWidget::removeNewMessagesSeparatorAfterTimeout (int timeoutMs)
{
	if (newMessagesSeparator) {
		removeNewMessagesSeparatorTimer.start (timeoutMs);
	}
}

void PostsListWidget::keyPressEvent (QKeyEvent* event)
{
	/*
	 * Handle the key sequence for 'Copy' and copy all posts to clipboard (properly formatted)
	 */
	if (event->matches (QKeySequence::Copy)) {
		QString str;
		for (auto& item: selectedItems()) {
			PostWidget* post = static_cast <PostWidget*> (itemWidget (item));
			str += post->formatForClipboardSelection ();
		}

		qDebug() << "Copy Posts Selection";
		QApplication::clipboard()->setText (str);
	}
}

void PostsListWidget::showContextMenu (const QPoint& pos)
{
	// Handle global position
	QPoint globalPos = mapToGlobal(pos);

	QListWidgetItem* pointedItem = itemAt(pos);
	PostWidget* post = static_cast <PostWidget*> (itemWidget (pointedItem));

	if (post->isDeleted) {
		return;
	}

	// Create menu and insert some actions
	QMenu myMenu;

	if (post->post.isOwnPost()) {
		myMenu.addAction ("Edit", [post] {
			qDebug() << "Edit " << post->post.message;
		});

		myMenu.addAction ("Delete", [this, post] {
			qDebug() << "Delete " << post->post.message;
			backend->deletePost (post->post.id);
		});

		myMenu.addSeparator();
	}

	myMenu.addAction ("Copy to clipboard", [post] {
		qDebug() << "Copy " << post->post.message;
	});

	myMenu.addAction ("Reply", [post] {
		qDebug() << "Reply " << post->post.message;
	});

	myMenu.addAction ("Pin", [post] {
		qDebug() << "Pin " << post->post.message;
	});


	// Show context menu at handling position
	myMenu.exec(globalPos + QPoint (10, 0));
}

void PostsListWidget::resizeEvent (QResizeEvent* event)
{
	for (int i = 0; i < count(); ++i) {
		QListWidgetItem* item = this->item(i);
		QWidget* widget = (QWidget*)itemWidget (item);

		if (!widget) {
			qDebug() << "ResizeEvent: Item has null widget";
			return;
		}

		if (widget->heightForWidth(event->size().width()) != -1) {
			item->setSizeHint(QSize (event->size().width(), widget->heightForWidth(event->size().width())));
			//qDebug() << "SetSizeHint " << event->size().width() << " " << widget->heightForWidth(event->size().width());
		}
	}

	QListWidget::resizeEvent (event);

	if (verticalScrollBar()->maximum() - verticalScrollBar()->value() < 10) {
		scrollToBottom ();
	}
}


} /* namespace Mattermost */
