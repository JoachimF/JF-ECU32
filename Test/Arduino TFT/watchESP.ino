
#include <TFT_eSPI.h> 
#include "fonts.h"
//#include "test.h"
//#include "time.h"
//#include "RTClib.h"

//RTC_DS3231 rtc;

TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite img = TFT_eSprite(&tft);
//TFT_eSprite txt = TFT_eSprite(&tft);
TFT_eSprite needle_egt = TFT_eSprite(&tft); // Sprite object for needle
TFT_eSprite needle_rpm = TFT_eSprite(&tft); // Sprite object for needle
//uint16_t* tft_buffer;
//uint16_t  spr_width = 0;
//bool buffer_loaded = false;
//uint16_t  bg_color =0;
char temp[6] ;

#define DEG2RAD 0.0174532925
#define AA_FONT_LARGE NotoSansBold36
#define color1 TFT_WHITE
#define color2  0x8410
#define color3 0x5ACB
#define color4 0x15B3
#define color5 0x00A3

#define Maxtemp 750
#define Mintemp 50
#define Maxrpm  140000
#define Idlerpm 35000
#define Minrpm  5000
#define Maxtemp10 Maxtemp*1.1
#define Maxrpm10 Maxrpm*1.1
#define EGT_INTERVALS 10
#define EGT_INTERVAL 100
#define RPM_INTERVALS 10
#define RPM_INTERVAL 20

#define NEEDLE_LENGTH 10  // Visible length
#define NEEDLE_WIDTH  5  // Width of needle - make it an odd number
#define NEEDLE_RADIUS 84 // Radius at tip
#define NEEDLE_COLOR1 TFT_WHITE  // Needle periphery colour
#define NEEDLE_COLOR2 TFT_WHITE     // Needle centre colour
#define DIAL_CENTRE_X 120
#define DIAL_CENTRE_Y 120

volatile int counter = 0;
float VALUE;
float lastValue=0;

double rad=0.01745;
float x[360];
float y[360];


float px[360];
float py[360];

float lx[360];
float ly[360];


int r=104;
int sx=120;
int sy=120;

String cc[12]={"45","40","35","30","25","20","15","10","05","0","55","50"};
String days[]={"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
String status[] = {"WAIT","START","GLOW","KEROSTART","PREHEAT","RAMP","IDLE","PURGE","COOL"};
int EGT_values[EGT_INTERVALS] ;
int RPM_values[RPM_INTERVALS] ;
int start[12];
int startP[60];
int EGT[EGT_INTERVALS];
int RPM[RPM_INTERVALS] ;

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

int status_width ;
int alarm_width ;
int pump_width ;
int titre_width ;
int egt_rpm_width ;

int angle=0;
bool onOff=0;
bool debounce=0;

String h,m,s,d1,d2,m1,m2;

void setup() {

 int temp_interval = EGT_INTERVAL;//(Maxtemp10 - Mintemp) / 9 ;
 int intervale ;
 Serial.begin(115200);
 Serial.println("Starting....") ;

 Serial.println("EGT_values[i]") ;
 for(int i=0;i<EGT_INTERVALS;i++)
 {
   EGT_values[i] = (Mintemp + i*temp_interval)/10 ;
   Serial.print(EGT_values[i]) ;
   Serial.print(";") ;
 }
Serial.println(" ") ;

 Serial.println("RPM_values[i]") ;

intervale = ((Maxrpm10 - Minrpm)/(RPM_INTERVALS-2))/1000 ; 
  for(int i=0;i<RPM_INTERVALS;i++)
 {
   RPM_values[i] = (Minrpm/1000 + i*intervale) ;
   Serial.print(RPM_values[i]) ;
   Serial.print(";") ;
 }
 Serial.println(" ") ;

    pinMode(2,OUTPUT);
    pinMode(0,INPUT_PULLUP);
    pinMode(35,INPUT_PULLUP);
    pinMode(13,INPUT_PULLUP);

    digitalWrite(2,0);

    tft.init();
    tft.fillScreen(TFT_BLACK);

  
  img.setTextFont(2);
  status_width = img.textWidth("-KEROSTART-");
  alarm_width = img.textWidth("-ALARM-");
  pump_width = img.textWidth(".7777.");
  titre_width = img.textWidth("Temp. x10 - RPM x1000");
  egt_rpm_width = img.textWidth("-777C-") ;

  
  //img.setSwapBytes(true);
  img.createSprite(240, 240);
  img.setTextDatum(4);

  tft.setPivot(DIAL_CENTRE_X, DIAL_CENTRE_Y);
    // Create the needle Sprite
  createNeedles();
    // Reset needle position to 0
  plotNeedles(0, 0);
    //delay(2000);

int b=0;
int b2=0;
int b3=0;
int b4=5;


    for(int i=0;i<360;i++)
    {
       x[i]=(r*cos(rad*i))+sx;
       y[i]=(r*sin(rad*i))+sy;
       px[i]=((r-11)*cos(rad*i))+sx;
       py[i]=((r-11)*sin(rad*i))+sy;
       lx[i]=((r-13)*cos(rad*i))+sx;
       ly[i]=((r-13)*sin(rad*i))+sy;
       
       if(i%30==0){
       start[b]=i;
       b++;
       }
       
       if(i > 100 && i < 240 )
       {
        if(i%12==0){
        EGT[b3]=i;
        b3++;
        }
       }

       if(i > 0 && i <= 80 )
       {
        if(i%12==0){
        RPM[b4]=i;
        Serial.print(b4);
        Serial.print("-") ;
        Serial.print(RPM[b4]);
        Serial.print(";") ;
        b4--;
        if(b4 <0 )b4 = 9;
        }
       }
       if(i > 320 && i <= 360 )
       {
        if(i%12==0){
        RPM[b4]=i;
        Serial.print(b4);
        Serial.print("-") ;
        Serial.print(RPM[b4]);
        Serial.print(";") ;
        b4--;
        if(b4 <0 )b4 = 9;
        }
       }

       if(i%6==0){
       startP[b2]=i;
       b2++;
       }
      }
Serial.println(" ") ;
Serial.println("RPM");
for(int i=0;i<11 ; i++)
{
        Serial.print(RPM[i]);
        Serial.print(";") ;
}
Serial.println(" ") ;
Serial.println("EGT");
for(int i=0;i<11 ; i++)
{
        Serial.print(EGT[i]);
        Serial.print(";") ;
}
Serial.println(" ") ;
draw_statics() ;
}

int lastAngle=0;
float circle=100;
bool dir=0;
int rAngle=359;

void draw_statics()
{
  int triangles_red,triangles_green,angle_red,angle_green ;

  triangles_green  = (Maxtemp/10 * 36 / 95)-1 ;
  triangles_red = 36 - triangles_green ;
  angle_red = 200 + triangles_green*3 ;

  // EGT            
  fillArc(DIAL_CENTRE_X, DIAL_CENTRE_Y, 200,triangles_green, 90,90, 3, TFT_GREEN);
  fillArc(DIAL_CENTRE_X, DIAL_CENTRE_Y, angle_red, triangles_red, 90,90, 3, TFT_RED);
  // RPM
  fillArc(DIAL_CENTRE_X, DIAL_CENTRE_Y, 52, 6, 90,90, 3, TFT_RED);
  fillArc(DIAL_CENTRE_X, DIAL_CENTRE_Y, 70, 30, 90,90, 3, TFT_BLUE);
  img.setTextFont(1) ;
  for(int i=0;i<EGT_INTERVALS;i++)
    {
       //txt.drawString(EGT_values[i],x[start[i]+angle],y[start[i]+angle],2);
       img.drawNumber(EGT_values[i],x[EGT[i]],y[EGT[i]]);
       img.drawNumber(RPM_values[i],x[RPM[i]],y[RPM[i]]);
       img.drawLine(px[EGT[i]],py[EGT[i]],lx[EGT[i]],ly[EGT[i]],color1);
       img.drawLine(px[RPM[i]],py[RPM[i]],lx[RPM[i]],ly[RPM[i]],color1);
     }


  img.pushSprite(0,0,0);
}

void loop() {
//Serial.println(millis());

  char str_egt[5] ;
  char str_rpm[5] ;
  static int count = 0 ;
  static int current_status = 0 ;
  int32_t millis_prec = millis();
  //uint16_t angle_egt = random(950); // random speed in range 0 to 240
  //uint16_t angle_rpm = random(200); // random speed in range 0 to 240
  static uint16_t angle_egt ; // random speed in range 0 to 240
  static uint16_t angle_rpm ; // random speed in range 0 to 240
  int time_to_draw ;
  
  //sprintf(str_egt,"%dC",angle_egt);
  //sprintf(str_rpm,"%dK",angle_rpm);  
  sprintf(str_egt,"%dC",angle_egt*10);
  sprintf(str_rpm,"%dK",angle_rpm);  
  angle=0;//now.second()*6; 
  // Plot needle at random angle in range 0 to 240, speed 40ms per increment
  //img.fillSprite(TFT_BLACK);

  //spr.drawString("Temp. x10 - RPM x1000",spr_width/2, spr.fontHeight()/2) ;
  
  img.fillCircle(DIAL_CENTRE_X,DIAL_CENTRE_Y,NEEDLE_RADIUS+1,1) ;
  img.setTextFont(2) ;
  img.fillRect(50,120,egt_rpm_width,img.fontHeight()+2,color3);
  img.fillRect(147,120,egt_rpm_width,img.fontHeight()+2,color3);
  img.fillRect(120-status_width/2,60-img.fontHeight()/2-1,status_width,img.fontHeight()+1,TFT_ORANGE);
  
  img.setTextColor(TFT_WHITE,TFT_BLACK);
  img.drawRect(120-pump_width/2,230-img.fontHeight()/2-1,pump_width,img.fontHeight()+2,TFT_WHITE);
  img.drawString("Pump",120,210,1) ;
  img.setTextColor(0x35D7,TFT_BLACK);
  img.drawString("EGT",70,125-img.fontHeight(),2);
  img.drawString("RPM",167,125-img.fontHeight(),2);
  img.setTextColor(TFT_WHITE,TFT_BLACK);
  img.setTextFont(1) ;
  img.drawString("Batt.",95,155,1) ;
  img.drawString("RC",150,155,1) ;
  img.setTextFont(2) ;
  img.drawRect(150-pump_width/2,170-img.fontHeight()/2-1,pump_width,img.fontHeight()+2,TFT_WHITE);
  img.drawRect(95-pump_width/2,170-img.fontHeight()/2-1,pump_width,img.fontHeight()+2,TFT_WHITE);
  
  

  img.setTextFont(2) ;
  img.setTextColor(1,TFT_ORANGE);
  img.drawString(status[current_status],120,60) ;
  if(angle_egt > Maxtemp/10)
  {
    img.fillRect(120-alarm_width/2,90-img.fontHeight()/2-1,alarm_width,img.fontHeight()+1,TFT_RED);
    img.setTextColor(TFT_WHITE,TFT_RED);
    img.drawString("ALARM",120,90) ;
    img.drawString("Overheat",120,110) ;
  }
  if(angle_rpm > Maxrpm/1000)
  {
    img.fillRect(120-alarm_width/2,90-img.fontHeight()/2-1,alarm_width,img.fontHeight()+1,TFT_RED);
    img.setTextColor(TFT_WHITE,TFT_RED);
    img.drawString("ALARM",120,90) ;
    img.drawString("Overspeed",120,110) ;
  }

  img.setTextColor(TFT_WHITE,1);  
  img.drawString("Temp. x10 - RPM x1000",120,30) ;
  img.drawString("72%",120,230) ;
  img.drawString("8.1V",95,170) ;
  img.drawString("1020",150,170) ;


  img.setTextColor(TFT_WHITE,color3);
  img.drawString(str_egt,75,120+img.fontHeight()/2+1,2);
  img.drawString(str_rpm,167,120+img.fontHeight()/2+1,2);

  img.pushSprite(0,0,0);
 // txt.pushSprite(0,0,0);
 // spr.pushSprite(120 - spr_width / 2, 25,0);

  plotNeedles(angle_egt, angle_rpm);
//  time_to_draw = millis()-millis_prec ;
//  Serial.print(time_to_draw) ;  
//  Serial.println("ms") ;  
  angle_egt +=5;
  angle_rpm +=5 ;
  current_status++ ;
  if(current_status > 8) current_status = 0 ;
  if(angle_egt > 95) angle_egt = 0 ;
  if(angle_rpm > Maxrpm10/1000) angle_rpm = 0 ;
/*  plotNeedles(EGT_values[count], RPM_values[count]);
count++ ;
if(count > 9)
  count = 0 ;
Serial.print(EGT_values[count]);
Serial.print(";") ;
Serial.print(RPM_values[count]);
Serial.println(";") ;
Serial.println(" ") ;*/
  // Pause at new position

delay(500);
}


void fillArc(int x, int y, int start_angle, int seg_count, int rx, int ry, int w, unsigned int colour)
{

  byte seg = 3; // Segments are 3 degrees wide = 120 segments for 360 degrees
  byte inc = 3; // Draw segments every 3 degrees, increase to 6 for segmented ring

  // Calculate first pair of coordinates for segment start
  float sx = cos((start_angle - 90) * DEG2RAD);
  float sy = sin((start_angle - 90) * DEG2RAD);
  uint16_t x0 = sx * (rx - w) + x;
  uint16_t y0 = sy * (ry - w) + y;
  uint16_t x1 = sx * rx + x;
  uint16_t y1 = sy * ry + y;

  // Draw colour blocks every inc degrees
  for (int i = start_angle; i < start_angle + seg * seg_count; i += inc) {

    // Calculate pair of coordinates for segment end
    float sx2 = cos((i + seg - 90) * DEG2RAD);
    float sy2 = sin((i + seg - 90) * DEG2RAD);
    int x2 = sx2 * (rx - w) + x;
    int y2 = sy2 * (ry - w) + y;
    int x3 = sx2 * rx + x;
    int y3 = sy2 * ry + y;

    img.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
    img.fillTriangle(x1, y1, x2, y2, x3, y3, colour);

    // Copy segment end to sgement start for next segment
    x0 = x2;
    y0 = y2;
    x1 = x3;
    y1 = y3;
  }
}

void createNeedles(void)
{
  needle_egt.setColorDepth(16);
  needle_rpm.setColorDepth(16);
  //needle.setSwapBytes(true);
  needle_egt.createSprite(NEEDLE_WIDTH, NEEDLE_LENGTH);  // create the needle Sprite
  needle_rpm.createSprite(NEEDLE_WIDTH, NEEDLE_LENGTH);  // create the needle Sprite

  needle_egt.fillSprite(TFT_BLACK); // Fill with black
  needle_rpm.fillSprite(TFT_BLACK); // Fill with black
  // Define needle pivot point relative to top left corner of Sprite
  uint16_t piv_x = NEEDLE_WIDTH / 2; // pivot x in Sprite (middle)
  uint16_t piv_y = NEEDLE_RADIUS;    // pivot y in Sprite
  needle_egt.setPivot(piv_x, piv_y);     // Set pivot point in this Sprite
  needle_rpm.setPivot(piv_x, piv_y);     // Set pivot point in this Sprite

  // Draw the red needle in the Sprite
  needle_egt.fillRect(0, 0, NEEDLE_WIDTH, NEEDLE_LENGTH, NEEDLE_COLOR1);
  needle_egt.fillRect(1, 1, NEEDLE_WIDTH-2, NEEDLE_LENGTH-2, NEEDLE_COLOR2);
  needle_rpm.fillRect(0, 0, NEEDLE_WIDTH-2, NEEDLE_LENGTH-2, NEEDLE_COLOR1);
  needle_rpm.fillRect(1, 1, NEEDLE_WIDTH-2, NEEDLE_LENGTH-2, NEEDLE_COLOR1);

}

// =======================================================================================
// Move the needle to a new position
// =======================================================================================
void plotNeedles(int16_t angle_egt, int16_t angle_rpm)
{
  static int16_t old_angle_egt = 90;//-120; // Starts at -120 degrees
  static int16_t old_angle_rpm = 90;//-120; // Starts at -120 degrees

  // Bounding box parameters
  static int16_t min_x;
  static int16_t min_y;
  static int16_t max_x;
  static int16_t max_y;

    // Update the number at the centre of the dial
         int new_egt = map(angle_egt,5,95,200,305) ;
         int new_rpm = map(angle_rpm,Minrpm/1000,Maxrpm10/1000,160,60) ;
         int new_maxegt = map(Maxtemp/10,5,95,200,305) ;
         int new_maxrpm = map(Maxrpm/1000,Minrpm/1000,Maxrpm10/1000,160,60) ;
         int new_idlerpm = map(Idlerpm/1000,Minrpm/1000,Maxrpm10/1000,160,60) ;

         if (new_egt < 200) new_egt = 200; // Limit angle to emulate needle end stops
         if (new_egt > 308) new_egt = 308;
         if (new_rpm < 52) new_rpm = 52; // Limit angle to emulate needle end stops
         if (new_rpm > 160) new_rpm = 160;

      needle_rpm.pushRotated(new_maxegt, TFT_BLACK);
      needle_egt.pushRotated(new_egt, TFT_BLACK);
      needle_egt.pushRotated(new_rpm, TFT_BLACK);
      needle_rpm.pushRotated(new_maxrpm, TFT_BLACK);
      needle_rpm.pushRotated(new_idlerpm, TFT_BLACK);
}
