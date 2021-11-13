#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTranslator>
#include <QTextCodec>
#include <utility>
#include "mainUI.h"

auto PathToAbsolute(QString path) -> QString{
  //Convert an input path to an absolute path (this does not check existance ot anything)
  if(path.startsWith("/")){ return path; } //already an absolute path
  if(path.startsWith("~")){ path.replace(0,1,QDir::homePath()); }
  if(!path.startsWith("/")){
    //Must be a relative path
    if(path.startsWith("./")){ path = path.remove(2); }
    path.prepend( QDir::currentPath()+"/");
  }
  return path;
}

auto LoadTranslation(QApplication *app, const QString& appname, QString locale = "", QTranslator *cTrans = nullptr) -> QTranslator*{
   //Get the current localization
    QString langEnc = "UTF-8"; //default value
    QString langCode = std::move(locale); //provided locale
    if(langCode.isEmpty()){ langCode = getenv("LC_ALL"); }
    if(langCode.isEmpty()){ langCode = getenv("LANG"); }
    if(langCode.isEmpty()){ langCode = "en_US.UTF-8"; } //default to US english
    //See if the encoding is included and strip it out as necessary
    if(langCode.contains(".")){
      langEnc = langCode.section(".",-1);
      langCode = langCode.section(".",0,0);
    }
    //Now verify the encoding for the locale
    if(langCode =="C" || langCode=="POSIX" || langCode.isEmpty()){
      langEnc = "System"; //use the Qt system encoding
    }
    if(app !=nullptr){
      qDebug() << "Loading Locale:" << appname << langCode << langEnc;
      //If an existing translator was provided, remove it first (will be replaced)
      if(cTrans!=nullptr){ QApplication::removeTranslator(cTrans); }
      //Setup the translator
      cTrans = new QTranslator();
      //Use the shortened locale code if specific code does not have a corresponding file
      if(!QFile::exists(L_SHAREDIR+"/lumina-pdf/i18n/"+appname+"_" + langCode + ".qm") && langCode!="en_US" ){
        langCode.truncate( langCode.indexOf("_") );
      }
      QString filename = appname+"_"+langCode+".qm";
      //qDebug() << "FileName:" << filename << "Dir:" << LOS::LuminaShare()+"i18n/";
      if( cTrans->load( filename, L_SHAREDIR+"/lumina-pdf/i18n/" ) ){
        QApplication::installTranslator( cTrans );
      }else{
	//Translator could not be loaded for some reason
	cTrans = nullptr;
	if(langCode!="en_US"){
	  qWarning() << " - Could not load Locale:" << langCode;
	}
      }
    }else{
      //Only going to set the encoding since no application given
      qDebug() << "Loading System Encoding:" << langEnc;
    }
    //Load current encoding for this locale
    QTextCodec::setCodecForLocale( QTextCodec::codecForName(langEnc.toUtf8()) );
    return cTrans;
}


auto main(int argc, char **argv) -> int {
  // LTHEME::LoadCustomEnvSettings();
  unsetenv("QT_AUTO_SCREEN_SCALE_FACTOR"); // need pixel-perfect geometries
  QApplication a(argc, argv);
  LoadTranslation(&a, "l-pdf");

  // Read the input variables
  QString path = "";
  for (int i = 1; i < argc; i++) {
    path = PathToAbsolute(argv[i]);
    if (QFile::exists(path)) {
      break;
    } // already found a valid file
  }

  MainUI w;
  if (!path.isEmpty()) {
    w.loadFile(path);
  }
  w.show();
  int retCode = QApplication::exec();
  return retCode;
}
