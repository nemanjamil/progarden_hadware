#include "filename.h"
#define PINRESET D10


int foo(int a){
    return ++a;
}

boolean isNegative(int x){
  Serial.println("I can't print this from inside my INCLUDE FILE"); 
  if (x<0) return true;
  return false;
}

void paligasi(){
  if(digitalRead(LED_BUILTIN)){
      digitalWrite(LED_BUILTIN, LOW);
  }else{
      digitalWrite(LED_BUILTIN, HIGH);
  }
}

int anotherOdity(){
  char ch[5];
  memcpy(ch,"1",1);  
}

int udjiuconfValue(){
  int vrati = 0;
  if (digitalRead(PINRESET) == HIGH){
    int low_count = 0;
    for (int i = 0; i<3; i++)
    {
      if (digitalRead(PINRESET) == HIGH){
        low_count++;
        delay(200);
      }else{
        break;
      }
    }
    if (low_count > 2){
      vrati = 1;
    }
  }
  return vrati;
}


/*GlavnaKlasa::GlavnaKlasaParameter(const char *custom) {
  _id = NULL;
  _placeholder = NULL;
  _length = 0;
  _value = NULL;
  _customHTML = custom;
}

const char* GlavnaKlasa::getID() {
  return "qwe";
}
*/

