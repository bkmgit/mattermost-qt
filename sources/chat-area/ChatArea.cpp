
#include "ChatArea.h"

#include "ui_ChatArea.h"
#include "PostWidget.h"
#include "backend/Backend.h"
#include "log.h"

namespace Mattermost {

ChatArea::ChatArea (Backend& backend, BackendChannel& channel, QTreeWidgetItem* tree, QWidget *parent)
:QWidget(parent)
,ui(new Ui::ChatArea)
,backend (backend)
,channel (channel)
,treeItem (new QTreeWidgetItem (tree))
,outgoingPostCreator (*this, ui->textEdit)
,unreadMessagesCount (0)
,texteditDefaultHeight (80)
{
	//accept drag&drop attachments
	setAcceptDrops(true);

	ui->setupUi(this);
	ui->listWidget->backend = &backend;

	ui->title->setText (channel.display_name);
	treeItem->setText (0, channel.display_name);
	treeItem->setData(0, Qt::UserRole, QVariant::fromValue(this));
	setTextEditWidgetHeight (texteditDefaultHeight);

	QFont font1;
	font1.setPixelSize(14);
	font1.setBold (true);
	treeItem->setFont (1, font1);

	const Mattermost::BackendUser* user = backend.getStorage().getUserById (channel.name);

	if (user) {

		connect (user, &BackendUser::onAvatarChanged, [this, user] {
			setUserAvatar (*user);
		});

		if (!user->avatar.isEmpty()) {
			setUserAvatar (*user);
		}
	} else {
		ui->userAvatar->clear();
		ui->userAvatar->hide();
	}

	/*
	 * First, get the first unread post (if any). So that a separator can be inserted before it
	 */
	backend.retrieveChannelUnreadPost (channel, [this, &backend, &channel] (const QString& postId){
		lastReadPostId = postId;

		if (!postId.isEmpty()) {
			qDebug () << "Last Read post for " << channel.display_name << ": " << postId;
		}

		backend.retrieveChannelPosts (channel, 0, 200);

#if 0
		backend.retrieveChannelPosts (channel, 0, 200, [this, postId]() {
			fillChannelPosts (postId);
		});
#endif
	});

	connect (&channel, &BackendChannel::onViewed, [this] {
		LOG_DEBUG ("Channel viewed: " << this->channel.name);
		setUnreadMessagesCount (0);
		ui->listWidget->removeNewMessagesSeparatorAfterTimeout (1000);
	});

	connect (&channel, &BackendChannel::onNewPost, this, &ChatArea::appendChannelPost);

	connect (&channel, &BackendChannel::onNewPosts, this,  &ChatArea::fillChannelPosts);

	connect (&channel, &BackendChannel::onUserTyping, this, &ChatArea::handleUserTyping);

	connect (&channel, &BackendChannel::onPostDeleted, this, &ChatArea::handlePostDeleted);


	/*
	 * Send new post after pressing enter or clicking the 'Send' button
	 */
	connect (ui->sendButton, &QPushButton::clicked, [this] {
		outgoingPostCreator.sendPost (this->backend, this->channel);
	});

	connect (ui->textEdit, &MessageTextEditWidget::enterPressed, [this] {
		outgoingPostCreator.sendPost (this->backend, this->channel);
	});

	connect (ui->attachButton, &QPushButton::clicked, &outgoingPostCreator, &OutgoingPostCreator::onAttachButtonClick);

	connect (ui->splitter, &QSplitter::splitterMoved, [this] {
		texteditDefaultHeight = ui->splitter->sizes()[1];
	});

	connect (ui->textEdit, &MessageTextEditWidget::textChanged, [this] {
		QSize size = ui->textEdit->document()->size().toSize();

		int height = size.height() + 10;

		if (height < texteditDefaultHeight) {
			height = texteditDefaultHeight;
		}

		if (height > ui->textEdit->maximumHeight()) {
			height = ui->textEdit->maximumHeight();
		}

		setTextEditWidgetHeight (height);
	});
}

ChatArea::~ChatArea()
{
    delete ui;
}

void ChatArea::setUserAvatar (const BackendUser& user)
{
	QImage img = QImage::fromData (user.avatar).scaled (64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	ui->userAvatar->setPixmap (QPixmap::fromImage(img));

	if (channel.type == BackendChannel::directChannel) {
		treeItem->setIcon(0, QIcon(QPixmap::fromImage(QImage::fromData(user.avatar))));
	}
}

BackendChannel& ChatArea::getChannel ()
{
	return channel;
}

void ChatArea::fillChannelPosts (const ChannelMissingPosts& collection)
{
	QDate currentDate = QDateTime::currentDateTime().date();
	int insertPos = 0;
	int startPos = 0;
	int lastDaysAgo = INT32_MAX;

	for (const ChannelMissingPostsSequence& list: collection.postsToAdd) {

		if (!list.previousPostId.isEmpty()) {
			qDebug() << "Add posts after" << list.previousPostId;
		}

		insertPos = ui->listWidget->findPostByIndex (list.previousPostId, startPos);
		++insertPos;
		startPos = insertPos;

		for (auto& post: list.postsToAdd) {

			if (!list.previousPostId.isEmpty()) {
				qDebug() << "\tAdd post " << post->id;
			}

			QDateTime postTime = QDateTime::fromMSecsSinceEpoch (post->create_at);
			QDate postDate = postTime.date();

			int postDaysAgo = (postDate.daysTo (currentDate));

			if (postDaysAgo < lastDaysAgo) {
				lastDaysAgo = postDaysAgo;

				if (postDaysAgo < 3) {
					ui->listWidget->addDaySeparator (postDaysAgo);
					++insertPos;
				}
			}

			ui->listWidget->insertPost (insertPos, new PostWidget (backend, *post, ui->listWidget, this));
			++insertPos;

			if (post->id == lastReadPostId) {
				ui->listWidget->addNewMessagesSeparator ();
				++insertPos;
				++unreadMessagesCount;
			}

			//ui->listWidget->addDaySeparator (0);
		}
	}

	ui->listWidget->adjustSize();
	setUnreadMessagesCount (unreadMessagesCount);
}

void ChatArea::appendChannelPost (BackendPost& post)
{
	bool chatAreaHasFocus = treeItem->isSelected() && isActiveWindow ();

	if (!chatAreaHasFocus) {
		ui->listWidget->addNewMessagesSeparator ();
	}

	ui->listWidget->insertPost (new PostWidget (backend, post, ui->listWidget, this));

	ui->listWidget->adjustSize();
	ui->listWidget->scrollToBottom();

	//do not add unread messages count if the Chat Area is on focus
	if (chatAreaHasFocus) {
		return;
	}

	++unreadMessagesCount;
	setUnreadMessagesCount (unreadMessagesCount);
}

void ChatArea::sendNewPost ()
{

}

void ChatArea::handleUserTyping (const BackendUser& user)
{
	LOG_DEBUG (channel.display_name << ": " << user.getDisplayName() << " is typing");
}

void ChatArea::handlePostDeleted (const QString& postId)
{
	PostWidget* postWidget = ui->listWidget->findPost (postId);

	if (postWidget) {
		postWidget->markAsDeleted ();
		ui->listWidget->adjustSize();
	}
}

void ChatArea::onActivate ()
{
	ui->listWidget->scrollToUnreadPostsOrBottom ();
	ui->listWidget->adjustSize();

	backend.markChannelAsViewed (channel);

	for (BackendFile* file: filesToLoad) {

		if (!file->contents.isEmpty()) {
			continue;
		}

		backend.retrieveFile (file->id, [file] (const QByteArray& data) {
			file->contents = data;
			emit file->onContentsAvailable (data);
		});
	}
}

void ChatArea::onMainWindowActivate ()
{
	setUnreadMessagesCount (0);
	backend.markChannelAsViewed (channel);
}

void ChatArea::addFileToload (BackendFile* file)
{
	if (!treeItem->isSelected()) {
		filesToLoad.push_back (file);
		return;
	}

	//if the chat area is active, load files immediately"

	if (!file->contents.isEmpty()) {
		return;
	}

	backend.retrieveFile (file->id, [file] (const QByteArray& data) {
		file->contents = data;
		emit file->onContentsAvailable (data);
	});
}

void ChatArea::setUnreadMessagesCount (uint32_t count)
{
	unreadMessagesCount = count;

	if (count == 0) {
		treeItem->setText(1, "");
	} else {
		treeItem->setText(1, QString::number(count));
	}
}

void ChatArea::dragEnterEvent (QDragEnterEvent* event)
{
	outgoingPostCreator.onDragEnterEvent (event);
}

void ChatArea::dragMoveEvent (QDragMoveEvent* event)
{
	outgoingPostCreator.onDragMoveEvent (event);
}

void ChatArea::dropEvent (QDropEvent* event)
{
	outgoingPostCreator.onDropEvent (event);
}

QVBoxLayout& ChatArea::getAttachmentListParentWidget ()
{
	return *ui->verticalLayout;
}

void ChatArea::setTextEditWidgetHeight (int height)
{
	//set the size of the text input area only. The chat area will take the whole remaining part, because it has higher stretch factor
	ui->splitter->setSizes({1, height});
}

} /* namespace Mattermost */
