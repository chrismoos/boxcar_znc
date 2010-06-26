#include <main.h>
#include <Nick.h>
#include <Modules.h>
#include <User.h>
#include <FileUtils.h>
#include <curl/curl.h>

#define SETTINGS_FILE "settings"

class CBoxcarMod : public CModule {
private:
  CString m_apiEmail; /* E-Mail for Boxcar account */
  CString m_apiPassword; /* Password for Boxcar account */
  bool m_active;

  void loadPreference(CString &preference) {
    CString key, value;
    VCString str;

    if(preference.Split(":", str, false) == 2) {
      key = str[0];
      value = str[1].Base64Decode_n();
      
      if(key.Equals("api_email")) m_apiEmail = value;
      else if(key.Equals("api_pass")) m_apiPassword = value;
      else if(key.Equals("active") && value.Equals("true")) m_active = true;
    }
  }
  
  void writePreferences() {
    CString path(GetSavePath() + "/" + SETTINGS_FILE);
    CFile file(path);
    file.Open(O_WRONLY | O_CREAT);
    
    file.Write("api_email:" + m_apiEmail.Base64Encode_n() + "\n");
    file.Write("api_pass:" + m_apiPassword.Base64Encode_n() + "\n");
    file.Write("active:" + m_active ? CString("true").Base64Encode_n() : CString("false").Base64Encode_n() + "\n");
    
    file.Close();
  }
  
  void loadSettings() {
    CFile settings(GetSavePath() + "/" + SETTINGS_FILE);
    CString text;
    CString preference;

    if(settings.Exists()) {
      settings.Open();
      while(settings.ReadLine(preference)) {
        loadPreference(preference);
      }
    }
  }
  
  bool accountSetup() {
    return !(m_apiPassword.Equals("") || m_apiEmail.Equals(""));
  }
  
public:
    MODCONSTRUCTOR(CBoxcarMod) {
      m_active = false;
    }
    
    virtual ~CBoxcarMod() { }
    
    void sendNotification(const char *from, const char *msg) {
      if(!accountSetup() || !m_active) return;
      
      CString auth(m_apiEmail + ":" + m_apiPassword);
      CURL *curl = curl_easy_init();
      char *fromEncoded, *msgEncoded;
      CURLcode code;
      long responseCode;
      
      std::string postString;
      
      postString += "notification[from_screen_name]=";
      fromEncoded = curl_easy_escape(curl, from, strlen(from));
      postString += fromEncoded;
      postString += "&notification[message]=";
      msgEncoded = curl_easy_escape(curl, msg, strlen(msg));
      postString += msgEncoded;

      curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postString.c_str());
      curl_easy_setopt(curl, CURLOPT_URL, "https://boxcar.io/notifications");
      
      if((code = curl_easy_perform(curl)) != 0) {
        PutModule("Unable to send notification to boxcar: " + CString(curl_easy_strerror(code)));
      }
      else {
        /* Check response code */
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        if(responseCode == 401) {
          PutModule("Unable to send a notification. Invalid Boxcar credentails.");
          PutModule("Check your username and password and make sure you have the Growl service enabled.");
        }  
      }
      curl_free(fromEncoded);
      curl_free(msgEncoded);
      curl_easy_cleanup(curl);
    }
    
    virtual void OnClientLogin() {
      loadSettings();
    }

    virtual void OnModCommand(const CString& sCommand) {
      if(sCommand.Token(0).Equals("HELP")) {
        PutModule("This module sends notifications to the Boxcar API.");
        PutModule("Currently notifications are sent when a private message is received.\n");
        PutModule("Commands:\n");
        PutModule("SET email_address password - Sets your email and password for the Boxcar API. (required)");
        PutModule("STATUS - current status");
        PutModule("ACTIVATE - activate boxcar notifications");
        PutModule("DEACTIVATE - turn boxcar notifications off");
      }
      else if(sCommand.Token(0).Equals("SET")) {
        m_apiEmail = sCommand.Token(1);
        m_apiPassword = sCommand.Token(2);
        writePreferences();
        PutModule("Boxcar credentials set.");
      }
      else if(sCommand.Token(0).Equals("STATUS")) {
        if(accountSetup()) {
          PutModule("Your Boxcar credentials are set, e-mail: " + m_apiEmail + ".");
          
          if(m_active) {
            PutModule("Notifications are enabled.");
          }
          else {
            PutModule("Notifications are currently turned off.");
          }
        }
        else {
          PutModule("Your Boxcar credentials are not set.");
          PutModule("Please use the SET command to add your Boxcar credentials. Try HELP for more information.");
        }
      }
      else if(sCommand.Token(0).Equals("ACTIVATE")) {
        m_active = true;
        PutModule("Notifications are now on.");
        writePreferences();
      }
      else if(sCommand.Token(0).Equals("DEACTIVATE")) {
        m_active = false;
        PutModule("Notifications are disabled.");
        writePreferences();
      }
      else {
        PutModule("Invalid command. Try HELP for more information.");
      }
    }
    
    virtual EModRet OnPrivMsg(CNick &Nick,CString &sMessage) {
      sendNotification(Nick.GetNick().c_str(), sMessage.c_str());
      return CONTINUE;
    }
};

MODULEDEFS(CBoxcarMod, "Sends notifications to the Boxcar API.")