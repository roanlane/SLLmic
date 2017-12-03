#ifndef SLL_h
#define SLL_h

#include <arduino.h>
#include <avr/pgmspace.h>
#include <SLL.h>
//Подключение библиотеки обязательно, скачать можно к примеру с
// http://forum.arduino.cc/index.php/topic,38153.0.html
#include <fix_fft.h>

#ifndef sbi 
    #define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif 
    
#ifndef cbi 
    #define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif 

class SLL{
	public:
		bool Find_cross_correletion(byte , byte** , int );
		bool Find_fft_phase(byte , byte** );
		bool Find_fft_filter(byte ,byte** , int , int );
		int Find_ff_ph_delta(byte* ,int ,int );
		void Sig_to_sin(byte* , char* );
		int Find_cr_co_delta(byte* ,byte*, int );
		void Find_cr_co_angle(byte ,double ,double );

		byte **data_array;
		int num_mic;
		int size_double_arr;
		int size_data_array;
		int distanse_be_mic;
		byte type_algorithm;
		int min_filter_limit;
		int max_filter_limit;
		byte division_factor;
		double freq_sampling; 
		double freq_sig;
		double freq_to_count;
		int voltage_thres;
		int num_max_count;
		int pin_array[4] = {0};
		int result_angle[2] = {0};
		int result_delta[2] = {0};
		int lenght[3] = {0};
		char* real_arr;
		char* image_arr;
		int* phase_arr;
		int* amp_arr;
		~SLL();		
		SLL(int*, int, int);
		void Initialization(byte, int, int, byte, int, int);
		int* Find_angle(int);
		int* Find_distance(int, int, int);
		int* Get_delta();

		#define CROSS_CORRELATION 0
		#define FFT_PHASE 1
		#define FFT_FILTER 2
		const float SPEED_SOUND = 340.29;
};

#endif