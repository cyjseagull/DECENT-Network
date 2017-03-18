
#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QString>
#include <QHeaderView>
#include <QTextStream>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <Qt>
#include <QColor>
#include <QMouseEvent>
#include <QTimer>

#include <iostream>
#include <vector>

#include "gui_wallet_tabcontentmanager.hpp"
#include "gui_wallet_global.hpp"


namespace gui_wallet
{
   class TransactionsTab : public TabContentManager {
      
      Q_OBJECT
   public:
      TransactionsTab();
      
      virtual void content_activated() {}
      virtual void content_deactivated() {}
      virtual void resizeEvent(QResizeEvent *a_event);
      
      void ArrangeSize();
      void SetInfo(std::string info_from_overview);
      
   public:
      QVBoxLayout       main_layout;
      QLabel            search_label;
      DecentTable       tablewidget;
      QLineEdit         user;
      int               green_row;
      
   private:
      std::string getAccountName(std::string accountId);
      
   public slots:
      void onTextChanged(const QString& text);
      void updateContents();
      void maybeUpdateContent();
      void currentUserChanged(std::string user);
      
   private:
      QTimer   m_contentUpdateTimer;
      bool     m_doUpdate = true;
      
      std::map<std::string, std::string> _user_id_cache;
      
      const std::vector<std::string> _table_columns = { "Time", "Type", "From", "To", "Amount", "Fee", "Description" };
   };
}

