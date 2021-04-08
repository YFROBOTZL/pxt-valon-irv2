#include "pxt.h"
#include <map>
#include <vector>
#include "ReceiverIR.h"
using namespace pxt;
typedef vector<Action> vA;

enum class Pins{
    P0=  3,
    P1=  2,
    P2=  1,
    P3=  4,
    P4=  5,
    P5=  17,
    P6=  12,
    P7=  11,
    P8=  18,
    P9=  10,
    P10= 6,
    P11= 26,
    P12= 20,
    P13= 23,
    P14= 22,
    P15= 21,
    P16= 16,
    P19= 0,
    P20= 30
};

enum class RemoteButton {
      MINIIR_A = 0x45,
	  MINIIR_B = 0x46,
	  MINIIR_C = 0x47,
	  MINIIR_D = 0x44,
	  MINIIR_up = 0x43,
	  MINIIR_E = 0xd,
	  MINIIR_left = 0x40,
	  MINIIR_set = 0x19,
	  MINIIR_right = 0x7,
	  MINIIR_NUM0 = 0x15,
	  MINIIR_down = 0x9,
	  MINIIR_F = 0x16,
	  MINIIR_NUM1 = 0xc,
	  MINIIR_NUM2 = 0x18,
	  MINIIR_NUM3 = 0x5e,
	  MINIIR_NUM4 = 0x8,
	  MINIIR_NUM5 = 0x1c,
	  MINIIR_NUM6 = 0x5a,
	  MINIIR_NUM7 = 0x42,
	  MINIIR_NUM8 = 0x52,
	  MINIIR_NUM9 = 0x4A,
};

namespace BitIR { 

    map<RemoteButton, vA> actions;
    map<RemoteButton, uint32_t> lastact;
    Timer tsb; 
    uint8_t buf[32];
    uint32_t now;
    ReceiverIR *rx;
    RemoteIR::Format fmt = RemoteIR::UNKNOWN;
    

    /* V2 logic */
    int ir_code = 0x00;
    int ir_addr = 0x00;
    int data;
    MicroBitPin* pIRV2_pin = NULL;


    void cA(vA runner){for(int i=0;i<runner.size();i++){runAction0(runner[i]);} }

    void onReceivable(){
        int x = rx->getData(&fmt, buf, 32 * 8);
        if(actions.find((RemoteButton)buf[2]) == actions.end()) return;
        now = tsb.read_ms();
        if(now - lastact[(RemoteButton)buf[2]] < 100) return;
        lastact[(RemoteButton)buf[2]] = now;
        cA(actions[(RemoteButton)buf[2]]);
    }

    void monitorIR(){
        while(1){
            while(rx->getState() != ReceiverIR::Received){
                uBit.sleep(50);
            }
            onReceivable();
        }
    }

    //%
    void init(Pins pin){
        rx = new ReceiverIR((PinName)pin);
        tsb.start(); //interrupt timer for debounce
        create_fiber(monitorIR);
    }

    //%
    void onPressEvent(RemoteButton btn, Action body) {
        actions[btn].push_back(body);
    }

    /********************************************
        Mini_IRV2 test
    *********************************************/
    int logic_value(){//判断逻辑值"0"和"1"子函数
        uint32_t lasttime = system_timer_current_time_us();
        uint32_t nowtime;
        while(!uBit.io.P4.getDigitalValue());//低等待
        nowtime = system_timer_current_time_us();
        if((nowtime - lasttime) > 400 && (nowtime - lasttime) < 700){//低电平560us
            while(uBit.io.P4.getDigitalValue());//是高就等待
            lasttime = system_timer_current_time_us();
            if((lasttime - nowtime)>400 && (lasttime - nowtime) < 700){//接着高电平560us
                return 0;
            }else if((lasttime - nowtime)>1500 && (lasttime - nowtime) < 1800){//接着高电平1.7ms
                return 1;
        }
        }
        uBit.serial.printf("error\r\n");
        return -1;
    }

    void pulse_deal(){
        int i;
        ir_addr=0x00;//清零
        for(i=0; i<16;i++ )
        {
            if(logic_value() == 1)
            {
                ir_addr |=(1<<i);
            }
        }
        //解析遥控器编码中的command指令
        ir_code=0x00;//清零
        for(i=0; i<16;i++ )
        {
            if(logic_value() == 1)
            {
                ir_code |=(1<<i);
            }
        }

    }

    void remote_decode(void){
        data = 0x00;
        uint32_t lasttime = system_timer_current_time_us();
        uint32_t nowtime;
        while(uBit.io.P4.getDigitalValue()){//高电平等待
            nowtime = system_timer_current_time_us();
            if((nowtime - lasttime) > 100000){//超过100 ms,表明此时没有按键按下
                ir_code = 0xff00;
                return;
            }
        }
        //如果高电平持续时间不超过100ms
        lasttime = system_timer_current_time_us();
        while(!uBit.io.P4.getDigitalValue());//低等待
        nowtime = system_timer_current_time_us();
        if((nowtime - lasttime) < 10000 && (nowtime - lasttime) > 8000){//9ms
            while(uBit.io.P4.getDigitalValue());//高等待
            lasttime = system_timer_current_time_us();
            if((lasttime - nowtime) > 4000 && (lasttime - nowtime) < 5000){//4.5ms,接收到了红外协议头且是新发送的数据。开始解析逻辑0和1
                pulse_deal();
                //uBit.serial.printf("addr=0x%X,code = 0x%X\r\n",ir_addr,ir_code);
                data = ir_code;
                return;//ir_code;
            }else if((lasttime - nowtime) > 2000 && (lasttime - nowtime) < 2500){//2.25ms,表示发的跟上一个包一致
                while(!uBit.io.P4.getDigitalValue());//低等待
                nowtime = system_timer_current_time_us();
                if((nowtime - lasttime) > 500 && (nowtime - lasttime) < 700){//560us
                    //uBit.serial.printf("addr=0x%X,code = 0x%X\r\n",ir_addr,ir_code);
                    data = ir_code;
                    return;//ir_code;
                }
            }
        }
    }

    //% 
    int irCode(){
        remote_decode();
        return data;
    }
  
}