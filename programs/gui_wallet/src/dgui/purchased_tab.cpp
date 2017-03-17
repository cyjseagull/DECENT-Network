/*
 *	File: decent_wallet_ui_gui_purchasedtab.hpp
 *
 *	Created on: 11 Feb 2017
 *	Created by: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *  This file implements ...
 *
 */

#include "purchased_tab.hpp"
#include <QHeaderView>
#include <QPushButton>
#include <QFileDialog>
#include <iostream>
#include <graphene/chain/config.hpp>
#include "json.hpp"
#include "gui_wallet_global.hpp"
#include <QMessageBox>


using namespace gui_wallet;
using namespace nlohmann;


static const char* s_vccpItemNames[]={" ", "Title", "Rating", "Size", "Price", "Created", "Status", "Progress"};

static const int   s_cnNumberOfCols = sizeof(s_vccpItemNames)/sizeof(const char*);


PurchasedTab::PurchasedTab()
        :
        m_pTableWidget(new P_TableWidget(0,s_cnNumberOfCols))
{

    PrepareTableWidgetHeaderGUI();

    QHBoxLayout* search_lay = new QHBoxLayout();

    m_filterLineEditer.setPlaceholderText(QString("Enter the term to search in title and description"));
    m_filterLineEditer.setStyleSheet("border: 1px solid white");
    m_filterLineEditer.setFixedHeight(40);
    m_filterLineEditer.setAttribute(Qt::WA_MacShowFocusRect, 0);

    QPixmap image(":/icon/images/search.svg");

    QLabel* search_label = new QLabel();
    search_label->setSizeIncrement(100,40);
    search_label->setPixmap(image);
    
    

    search_lay->setContentsMargins(42, 0, 0, 0);
    search_lay->addWidget(search_label);
    search_lay->addWidget(&m_filterLineEditer);

    m_pTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_pTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_main_layout.setContentsMargins(0, 0, 0, 0);

    m_main_layout.addLayout(search_lay);
    m_main_layout.addWidget(m_pTableWidget);

    setLayout(&m_main_layout);
    
    
    connect(&m_filterLineEditer, SIGNAL(textChanged(QString)), this, SLOT(onTextChanged(QString)));
    connect(m_pTableWidget, SIGNAL(mouseMoveEventDid), this, SLOT(doRowColor()));
    for(int i = 0; i < m_pTableWidget->rowCount(); ++i)
    {
        connect((PButton*)m_pTableWidget->cellWidget(i,0),SIGNAL(mouseWasMoved()),this,SLOT(doRowColor()));
    }
    
    m_contentUpdateTimer.connect(&m_contentUpdateTimer, SIGNAL(timeout()), this, SLOT(maybeUpdateContent()));
    m_contentUpdateTimer.setInterval(1000);
}



void PurchasedTab::maybeUpdateContent() {
    m_contentUpdateTimer.stop();
    updateContents();
    m_contentUpdateTimer.start();
}

void PurchasedTab::onTextChanged(const QString& text) {
    
    m_doUpdate = true;
}

void PurchasedTab::updateContents() {
    
    
    auto& global_instance = gui_wallet::GlobalEvents::instance();
    std::string str_current_username = global_instance.getCurrentUser();
    
    if(str_current_username == "") {return;} // if key not imported
    
    std::string a_result;
    RunTask("get_buying_history_objects_by_consumer_term "
               "\"" + str_current_username +"\" "
               "\"" + m_filterLineEditer.text().toStdString() +"\"", a_result);
    
    
    if(last_contents == a_result)
    {
        return;
    }
    
    
    try {
        auto contents = json::parse(a_result);
        last_contents = a_result;
        if (contents.size() + 1 != m_pTableWidget->rowCount()) {
          m_pTableWidget->setRowCount(0); //Remove everything but header
          m_pTableWidget->setRowCount(contents.size());
           
        }
       
        for (int i = 0; i < contents.size(); ++i) {
            
            auto content = contents[i];
            
            
            std::string time = contents[i]["expiration_time"].get<std::string>();
            
            std::string synopsis = unescape_string(contents[i]["synopsis"].get<std::string>());
            std::replace(synopsis.begin(), synopsis.end(), '\t', ' '); // JSON does not like tabs :(
            std::replace(synopsis.begin(), synopsis.end(), '\n', ' '); // JSON does not like tabs :(

            try {
                auto synopsis_parsed = json::parse(synopsis);
                synopsis = synopsis_parsed["title"].get<std::string>();
            } catch (...) {}
            
            double rating = contents[i]["rating"].get<double>() / 1000;
            uint64_t size = contents[i]["size"].get<int>();


            double price = 0;
            if (contents[i]["price"]["amount"].is_number()){
                price =  contents[i]["price"]["amount"].get<double>();
            } else {
                price =  std::stod(contents[i]["price"]["amount"].get<std::string>());
            }
            price /= GRAPHENE_BLOCKCHAIN_PRECISION;
            
            std::string expiration_or_delivery_time = contents[i]["expiration_or_delivery_time"].get<std::string>();
            std::string URI = contents[i]["URI"].get<std::string>();
            
            QLabel* imag_label = new QLabel();
            imag_label->setAlignment(Qt::AlignCenter);
            
            QPixmap image1(":/icon/images/info1.svg");
            
            
            SDigitalContent contentObject;
            std::string dcresult;
            RunTask("get_content \"" + URI + "\"", dcresult);
            
            auto dcontent_json = json::parse(dcresult);
            
            if (content["delivered"].get<bool>()) {
                contentObject.type = DCT::BOUGHT;
            } else {
                contentObject.type = DCT::WAITING_DELIVERY;
            }
            
            contentObject.author = dcontent_json["author"].get<std::string>();
            contentObject.price.asset_id = dcontent_json["price"]["asset_id"].get<std::string>();
            contentObject.synopsis = dcontent_json["synopsis"].get<std::string>();
            contentObject.URI = dcontent_json["URI"].get<std::string>();
            contentObject.created = dcontent_json["created"].get<std::string>();
            contentObject.expiration = dcontent_json["expiration"].get<std::string>();
            contentObject.size = dcontent_json["size"].get<int>();
            
            if (dcontent_json["times_bougth"].is_number()) {
                contentObject.times_bougth = dcontent_json["times_bougth"].get<int>();
            } else {
                contentObject.times_bougth = 0;
            }
            
            
            if (dcontent_json["price"]["amount"].is_number()){
                contentObject.price.amount =  dcontent_json["price"]["amount"].get<double>();
            } else {
                contentObject.price.amount =  std::stod(dcontent_json["price"]["amount"].get<std::string>());
            }
            
            contentObject.price.amount /= GRAPHENE_BLOCKCHAIN_PRECISION;
            contentObject.AVG_rating = dcontent_json["AVG_rating"].get<double>() / 1000;
        
            
            m_pTableWidget->horizontalHeader()->setStretchLastSection(true);
            m_pTableWidget->setCellWidget(i, 0, new TableWidgetItemW<PButton>(contentObject, this, NULL, &PurchasedTab::DigContCallback, tr("")));
            ((PButton*)m_pTableWidget->cellWidget(i,0))->setPixmap(image1);
            ((PButton*)m_pTableWidget->cellWidget(i,0))->setAlignment(Qt::AlignCenter);
            
            
            m_pTableWidget->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(synopsis)));
            m_pTableWidget->setItem(i, 2, new QTableWidgetItem(QString::number(rating)));
            m_pTableWidget->setItem(i, 3, new QTableWidgetItem(QString::number(size) + tr(" MB")));
            m_pTableWidget->setItem(i, 4, new QTableWidgetItem(QString::number(price) + " DCT"));
            
           

            std::string s_time;
            for(int i = 0; i < time.find("T"); ++i)
            {
                s_time.push_back(time[i]);
            }
            m_pTableWidget->setItem(i, 5, new QTableWidgetItem(QString::fromStdString(s_time)));
            
            
            std::string download_status_str;
            RunTask("get_download_status \"" + gui_wallet::GlobalEvents::instance().getCurrentUser() + "\" \"" + URI + "\"", download_status_str);
            
            auto download_status = json::parse(download_status_str);

            
            int total_key_parts = download_status["total_key_parts"].get<int>();
            int received_key_parts  = download_status["received_key_parts"].get<int>();
            int total_download_bytes  = download_status["total_download_bytes"].get<int>();
            int received_download_bytes  = download_status["received_download_bytes"].get<int>();
            
            
            QString status_text = tr("Keys: ") + QString::number(received_key_parts) + "/" + QString::number(total_key_parts);
            
            if (!content["delivered"].get<bool>()) {
                status_text = "Waiting for delivery";
            } else {
                status_text = status_text + tr(" ") + QString::fromStdString(download_status["status_text"].get<std::string>());
            }

            m_pTableWidget->setItem(i, 6, new QTableWidgetItem(status_text));
            
            if (total_key_parts == 0) {
                total_key_parts = 1;
            }
            
            if (total_download_bytes == 0) {
                total_download_bytes = 1;
            }
            
            
            double progress = (0.1 * received_key_parts) / total_key_parts + (0.9 * received_download_bytes) / total_download_bytes;
            progress *= 100; // Percent
            
            if (received_download_bytes < total_download_bytes) {
                m_pTableWidget->setItem(i, 7, new QTableWidgetItem(QString::number(progress) + "%"));
                m_pTableWidget->item(i, 7)->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
                m_pTableWidget->item(i, 7)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
            } else {
                EButton* btn = new EButton();
                btn->setStyleSheet("background-color: rgb(27,176,104); color: white");
                btn->setText("Extract");
                btn->setProperty("id", QVariant::fromValue(QString::fromStdString(content["id"].get<std::string>())));
                btn->setProperty("hash", QVariant::fromValue(QString::fromStdString(dcontent_json["_hash"].get<std::string>())));
                btn->setProperty("URI", QVariant::fromValue(QString::fromStdString(content["URI"].get<std::string>())));
                
                connect(btn, SIGNAL(clicked()), this, SLOT(extractPackage()));
                m_pTableWidget->setCellWidget(i, 7, btn);
                connect((EButton*)m_pTableWidget->cellWidget(i,7),SIGNAL(mouseMoved()),this,SLOT(doRowColor()));
            }
            
            
            for(int j = 1; j < s_cnNumberOfCols - 1; ++j)
            {
                m_pTableWidget->item(i, j)->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
                m_pTableWidget->item(i, j)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
            }
            

        }
        
        
    } catch (std::exception& ex) {
    }
    connect(m_pTableWidget, SIGNAL(mouseMoveEventDid), this, SLOT(doRowColor()));
    for(int i = 0; i < m_pTableWidget->rowCount(); ++i)
    {
        connect((PButton*)m_pTableWidget->cellWidget(i,0),SIGNAL(mouseWasMoved()),this,SLOT(doRowColor()));
    }
    
}

void PurchasedTab::extractPackage() {
    QPushButton* btn = (QPushButton*)sender();
    std::string id = btn->property("id").toString().toStdString();
    std::string URI = btn->property("URI").toString().toStdString();
    std::string hash = btn->property("hash").toString().toStdString();
    
    QString sampleDir = QFileDialog::getExistingDirectory(this, tr("Select directory to extract"), "~", QFileDialog::DontResolveSymlinks);

    std::string key, dummy;
    
    try {
        RunTask("restore_encryption_key \"" + id + "\"", key);
    
        RunTask("extract_package \"" + hash + "\" \"" + sampleDir.toStdString() + "\" " + key, dummy);
        MESSAGE("Package was successfully extracted");
        
    } catch (const std::exception& ex) {
        ALERT_DETAILS("Failed to extract package", ex.what());
    }
    
}


void PurchasedTab::DigContCallback(_NEEDED_ARGS2_)
{
    emit ShowDetailsOnDigContentSig(*a_pDigContent);
}


PurchasedTab::~PurchasedTab()
{
    m_main_layout.removeWidget(m_pTableWidget);
    delete m_pTableWidget;
}


void PurchasedTab::PrepareTableWidgetHeaderGUI()
{
    QFont font( "Open Sans Bold", 14, QFont::Bold);

    m_pTableWidget->horizontalHeader()->setDefaultSectionSize(300);
    m_pTableWidget->setRowHeight(0,35);
    m_pTableWidget->verticalHeader()->hide();
    m_pTableWidget->setHorizontalHeaderLabels(QStringList()  <<" " << "Title" << "Rating" << "Size" <<  "Price" <<  "Created" <<  "Status" << "Progress");
    m_pTableWidget->horizontalHeader()->setFixedHeight(35);
    m_pTableWidget->horizontalHeader()->setFont(font);
    m_pTableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_pTableWidget, SIGNAL(mouseMoveEventDid()), this, SLOT(doRowColor()));
    for(int i = 0; i < m_pTableWidget->rowCount(); ++i)
    {
        connect((PButton*)m_pTableWidget->cellWidget(i,0),SIGNAL(mouseWasMoved()),this,SLOT(doRowColor()));
    }
    m_pTableWidget->horizontalHeader()->setStyleSheet("QHeaderView::section {"
                                                   "border-right: 1px solid rgb(193,192,193);"
                                                   "border-bottom: 0px;"
                                                   "border-top: 0px;}");
}



void PurchasedTab::ArrangeSize()
{
    QSize tqsTableSize = m_pTableWidget->size();

    m_pTableWidget->setStyleSheet("QTableView{border : 0px}");
    m_pTableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_pTableWidget->setColumnWidth(0, tqsTableSize.width() * 0.1);
    
    for(int i = 0; i < s_cnNumberOfCols; ++i)
    {
        m_pTableWidget->setColumnWidth(i, (0.9 * tqsTableSize.width()) / (s_cnNumberOfCols - 1) );
    }
}


void PurchasedTab::resizeEvent ( QResizeEvent * a_event )
{
    QWidget::resizeEvent(a_event);
    ArrangeSize();
}



void PurchasedTab::doRowColor()
{
    for(int i = 0; i < m_pTableWidget->rowCount(); ++i)
    {
        if(m_pTableWidget->rowCount() < 0) {return;}
        
        for(int j = 1; j < 7; ++j)
        {
            if(m_pTableWidget->item(i,j) != NULL)
            {
                m_pTableWidget->item(i,j)->setBackground(QColor(255,255,255));
                m_pTableWidget->item(i,j)->setForeground(QColor::fromRgb(88,88,88));
            }
        }
        
        EButton* button_type = new EButton();
        button_type = qobject_cast<EButton*>(m_pTableWidget->cellWidget(i, 7));
        
        if( NULL == button_type )
        {
            if(m_pTableWidget->item(i,7) != NULL)
            {
                m_pTableWidget->item(i,7)->setBackground(QColor(255,255,255));
                m_pTableWidget->item(i,7)->setForeground(QColor::fromRgb(88,88,88));
            }
        }

        if(m_pTableWidget->cellWidget(i , 0) != NULL)
        {
            m_pTableWidget->cellWidget(i , 0)->setStyleSheet("* { background-color: rgb(255,255,255); color : white; }");
        }
    }
    QPoint mouse_pos = m_pTableWidget->mapFromGlobal(QCursor::pos());
    if(mouse_pos.x() > 0 && mouse_pos.x() < 110)
    {
        mouse_pos.setX(mouse_pos.x() + 300);
    }
    if(mouse_pos.x() > 790)
    {
        mouse_pos.setX(mouse_pos.x() - 500);
    }
    mouse_pos.setY(mouse_pos.y() - 41);
    QTableWidgetItem *ite = m_pTableWidget->itemAt(mouse_pos);
    
    if(ite != NULL)
    {
        int row = ite->row();
        if(row < 0) {return;}
        QPixmap image(":/icon/images/info1_white.svg");
        if(m_pTableWidget->cellWidget(row , 0) != NULL)
        {
            m_pTableWidget->cellWidget(row , 0)->setStyleSheet("* { background-color: rgb(27,176,104); color : white; }");
        }
        
        for(int i = 1 ; i < 7; ++i)
        {
            if(m_pTableWidget->item(row,i) != NULL)
            {
                m_pTableWidget->item(row,i)->setBackgroundColor(QColor(27,176,104));
                m_pTableWidget->item(row,i)->setForeground(QColor::fromRgb(255,255,255));
            }
        }
        QPushButton* button_type = new QPushButton();
        button_type = qobject_cast<QPushButton*>(m_pTableWidget->cellWidget(row, 7));
        if( NULL == button_type )
        {
            if(m_pTableWidget->item(row,7) != NULL)
            {
                m_pTableWidget->item(row,7)->setBackgroundColor(QColor(27,176,104));
                m_pTableWidget->item(row,7)->setForeground(QColor::fromRgb(255,255,255));
            }
        }
    }
}

void P_TableWidget::mouseMoveEvent(QMouseEvent * event)
{
    mouseMoveEventDid();
}
