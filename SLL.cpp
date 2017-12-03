/*Библиотека для звуковой локализации на основе микрофонной решётки.
*/
#include <SLL.h>
/**************************
Создание объекта SLL
f_pin_array - массив с номерами использумых пинов
f_num_mic - число используемых микрофонов - от 2 до 4
f_size_data_array - размер массива данных - степень двойки, если взять больше 128 
может не хватить памяти, для кросс корреляции достаточно 32, для FFT лучше брать
максимально возможное число
***************************/
SLL::SLL(int f_pin_array[], int f_num_mic = 4 , int f_size_data_array = 128){
	num_mic = f_num_mic;
	size_double_arr = 2;
	if (num_mic>2){
		size_double_arr = 3;
	}
	size_data_array = f_size_data_array;
	byte **f_data_array = new byte *[size_double_arr];
	for (int i=0; i<size_double_arr; i++){
		f_data_array[i] = new byte[size_data_array];
	}
	data_array = f_data_array;
	for (int i=0; i<size_double_arr; i++){
		for (int j=0 ; j<size_data_array; j++){
			data_array[i][j] = 0;
		}
	}
	for (int i=0; i<num_mic; i++){
		pin_array[i] = f_pin_array[i];
	}
	real_arr = new char[size_data_array] ;
	image_arr = new char[size_data_array];
	float ij = size_data_array/2.0;
	int half_size_data_array = ij;
	phase_arr = new int[half_size_data_array] ;
	amp_arr = new int[half_size_data_array] ;
}
/**************************
Дструктор очищает использыемую память
***************************/
SLL::~SLL(){
	for (int i=0; i<size_double_arr; i++){
		delete[] data_array[i];
	}
	delete[] data_array;
	delete[] real_arr, image_arr, phase_arr, amp_arr;
}
/****************************
Задание основных параметров работы алгоритма
f_division_factor - Division Factor задаёт частоту дискретизации АЦП микроконтроллера,
меньше 4 и больше 16 в данном алгоритме выбирать не рекомендуется, возможные значения:
2 4 8 16 32 64 128
f_distanse_be_mic - расстояние между микрофонами в см.
f_voltage_thres - на сколько отчётов квантователя сдвинут синусоидальный сигнал, если -1, то
число считается автоматически
f_type_algorithm - тип алгоритма определения углов
CROSS_CORRELATION 0 - кросс-корреляция
FFT_PHASE 1 - по спектрам сигнала
FFT_FILTER 2 - по спектрам с учёта ограничения по частотам:
	f_min_filter_limit - минимальная пропускаемая частота в Гц
	f_max_filter_limit - максимальная пропускаемая частота в Гц
В случае определения расстояния до источника звука - определяет метод вычисления направления на источник
звука на данном микроконтроллере
*******************************/
void SLL::Initialization(byte f_division_factor = 4, int f_distanse_be_mic=10, int f_voltage_thres = 0,
	byte f_type_algorithm = CROSS_CORRELATION, int f_min_filter_limit=20, int f_max_filter_limit = 6000){
	distanse_be_mic = f_distanse_be_mic;
	type_algorithm = f_type_algorithm;
	min_filter_limit = f_min_filter_limit;
	max_filter_limit = f_max_filter_limit;
	division_factor = log(f_division_factor)/log(2);
	if (division_factor&4)
		sbi(ADCSRA,ADPS2);
	else
		cbi(ADCSRA,ADPS2);
	if (division_factor&2)
		sbi(ADCSRA,ADPS1);
	else
		cbi(ADCSRA,ADPS1);
	if (division_factor&1)
		sbi(ADCSRA,ADPS0);
	else
		cbi(ADCSRA,ADPS0);
	unsigned long end_time = 0;
	unsigned long begin_time = micros();
	int val[2] = {0};
	for (int i = 0;i<10000;i++){
		val[0] = analogRead(pin_array[0]);
		val[1] = analogRead(pin_array[1]);
	}
	end_time = micros();
	end_time = end_time - begin_time;
	// Определение частоты АЦП
	freq_sampling = 1000000.0/end_time;
	freq_sampling = freq_sampling*10000;
	if(num_mic!=2)
		freq_sampling = freq_sampling/1.5;
	//Опередеелние числа отчётов АЦП, за которое сигнал пройдёт расстояние между микрофонами
	num_max_count = freq_sampling/(SPEED_SOUND*100.0/distanse_be_mic)-1;
	voltage_thres = f_voltage_thres;
	if (f_voltage_thres==-1){
		for (int i = 0;i<100;i++){
			voltage_thres = voltage_thres + analogRead(pin_array[0]);
		}
		voltage_thres = (voltage_thres+1)/100.0;
	}
}
/*********************************
Вычисление угловых координат источника звука
f_val_thres - величина порога срабатывания алгоритма, на звук меньшей величины алгоритм
не реагирует, выбирается эксперементально в зависимости от характера отслеживаемого сигнала,
общего уровня шума и чувствительности микрофонов
result_angle - возвращаемый массив углов, где первое значение - азимутальный угол, 
второе - угол наклона
*********************************/
int* SLL::Find_angle(int f_val_thres=40){
	int data_from_mic  = 0;
	int num_first_mic = -1;
	//В цикле ожидаем поступление информационного сигнала
	while(num_first_mic==-1){
	 	for (int i=0; i<num_mic; i++){
	 		data_from_mic = analogRead(pin_array[i]);
	 		if (data_from_mic>=f_val_thres+voltage_thres){
	 			num_first_mic = i;
	 			break;
	 		}
	 	}
	}
	/* Число строк в данных циклах возможно значительно сократить, однако делать это не 
	рекомендуется, поскольку усложнение кода в данном месте приведёт к уменьшению точности
	алгоритма из-за дополнительных затрат во времени */
	switch (num_mic){
	case 4:
		if(num_first_mic == 0){
			for (int j=0 ; j<size_data_array; j++){
				data_array[0][j] =  analogRead(pin_array[0]);
				data_array[1][j] =  analogRead(pin_array[1]);
				data_array[2][j] =  analogRead(pin_array[3]);
			}
		}
		if(num_first_mic == 1){
			for (int j=0 ; j<size_data_array; j++){
				data_array[0][j] =  analogRead(pin_array[1]);
				data_array[1][j] =  analogRead(pin_array[2]);
				data_array[2][j] =  analogRead(pin_array[0]);
			}
		}
		if(num_first_mic == 2){
			for (int j=0 ; j<size_data_array; j++){
				data_array[0][j] =  analogRead(pin_array[2]);
				data_array[1][j] =  analogRead(pin_array[3]);
				data_array[2][j] =  analogRead(pin_array[1]);
			}
		}
		if(num_first_mic == 3){
			for (int j=0 ; j<size_data_array; j++){
				data_array[0][j] =  analogRead(pin_array[3]);
				data_array[1][j] =  analogRead(pin_array[0]);
				data_array[2][j] =  analogRead(pin_array[2]);
			}
		}
		break;
	case 3:
		if(num_first_mic == 0){
			for (int j=0 ; j<size_data_array; j++){
				data_array[0][j] =  analogRead(pin_array[0]);
				data_array[1][j] =  analogRead(pin_array[1]);
				data_array[2][j] =  analogRead(pin_array[3]);
			}
		}
		if(num_first_mic == 1){
			for (int j=0 ; j<size_data_array; j++){
				data_array[0][j] =  analogRead(pin_array[1]);
				data_array[1][j] =  analogRead(pin_array[2]);
				data_array[2][j] =  analogRead(pin_array[0]);
			}
		}
		if(num_first_mic == 2){
			for (int j=0 ; j<size_data_array; j++){
				data_array[0][j] =  analogRead(pin_array[2]);
				data_array[1][j] =  analogRead(pin_array[0]);
				data_array[2][j] =  analogRead(pin_array[1]);
			}
		}
		break;
	default:
		Serial.println("Error num mic");
		break;
	}
	// Конец цикла 
	int flag = 0;
	switch(type_algorithm){
		case CROSS_CORRELATION:
			flag = !SLL::Find_cross_correletion(num_first_mic, &data_array[0], f_val_thres)*1;
			break;
		case FFT_PHASE:
			flag = !SLL::Find_fft_phase(num_first_mic, &data_array[0])*2;
			break;
		case FFT_FILTER:
			flag = !SLL::Find_fft_filter(num_first_mic, &data_array[0], min_filter_limit, max_filter_limit)*3;
			break;
		default:
			flag = 4;
			break;
	}	
	Serial.print(" freq_sampling");
	Serial.print(freq_sampling);	
	Serial.print(" num_max_count");
	Serial.print(num_max_count);
	Serial.print("num_first_mic");
	Serial.print(num_first_mic);	
	Serial.print("angle_phi");
	Serial.print(result_angle[0]);
	Serial.print("angle_qu");
	Serial.print(result_angle[1]);
	Serial.print("result_delta");
	Serial.print(result_delta[0]);
	Serial.print("result_delta");
	Serial.println(result_delta[1]);
	switch(flag){
		case 1:
			Serial.println("Error, the wrong value threshold");
		case 3:
			Serial.println("Error, the wrong frequency range");
		case 4:
			Serial.println("Error, does not exist type of algorithm");
	}
	return result_angle;
}	
/******************************
Вычисление угловых координат с помощью функции кросс-корреляции
Возвращает 1, если фукнкция выполнена корректно
*******************************/
bool SLL::Find_cross_correletion(byte num_first_mic, byte** f_data_array, int f_val_thres){
	if(f_val_thres<=0|| f_val_thres>=255)
		return 0;
	int delta_1 = SLL::Find_cr_co_delta(&f_data_array[0][0], &f_data_array[1][0], f_val_thres);
	int delta_2 =  SLL::Find_cr_co_delta(&f_data_array[0][0], &f_data_array[2][0], f_val_thres);
 	Find_cr_co_angle(num_first_mic, delta_1, delta_2);
 	result_delta[0]  = delta_1;
 	result_delta[1] = delta_2 ;
 	return 1;
}	

/******************************
Вычисление угловых координат с помощью преобразования фурье
Возвращает 1, если фукнкция выполнена корректно, задаёт диапазон рассматриваемых
частот поддерживаемых АЦП
*******************************/
bool SLL::Find_fft_phase(byte num_first_mic, byte** f_data_array){
	return SLL::Find_fft_filter(num_first_mic, f_data_array, 20, freq_sampling/4.0);

}
/******************************
Вычисление угловых координат с помощью преобразования фурье с учётом частот фильтрации
Возвращает 1, если фукнкция выполнена корректно, в случае не верного заданияя диапазона
частот можно потерять информационный сигнал
*******************************/
bool SLL::Find_fft_filter(byte num_first_mic,byte **f_data_array, int min_filter_limit=200, int max_filter_limit = 2000){
	if (max_filter_limit>freq_sampling || min_filter_limit<0 || max_filter_limit<=min_filter_limit)
		return 0;
	int delta_1 = SLL::Find_ff_ph_delta(&f_data_array[0][0],min_filter_limit,max_filter_limit);
	int delta_2 = SLL::Find_ff_ph_delta(&f_data_array[1][0],min_filter_limit,max_filter_limit);
	int delta_3 = SLL::Find_ff_ph_delta(&f_data_array[2][0],min_filter_limit,max_filter_limit);
	if(delta_2>delta_1){
    	delta_2 = delta_2-360;
    }
    if(delta_3>delta_1){
    	delta_3 = delta_3-360;
    }
    /*Перевод фазы сигнала в число отчётов АЦП с учётом определённой частоты сигнала freq_sig
    и  расстояния между микрофонами distanse_be_mic*/
    delta_2 = (delta_1-delta_2);
    delta_3 = (delta_1-delta_3); 
    float ij = delta_2*SPEED_SOUND*100.0/distanse_be_mic/freq_sig/360*num_max_count; 
    delta_2 = ij;
    ij = delta_3*SPEED_SOUND*100.0/distanse_be_mic/freq_sig/360*num_max_count;
    delta_3 = ij;
    /*Вычисления угловых координат тем же способом, что и для кросс-корреляции
	ВАЖНО!!!, возможно иcпользование непосредственно фаз сигналов без перевода их в отчёты,
	однако в этомм случае возможно вычисление для плоской решётки только азимутального угла. 
	Для этого закомментируйте две предыдущие строки*/
    SLL::Find_cr_co_angle(num_first_mic, delta_2, delta_3);
 	result_delta[0]  = delta_2;
 	result_delta[1] = delta_3;  
 	return 1;
}
/******************************
Вычисление фазового и сплитудного спектров сигнала и определение фаз информационного сигнала
Возвращает значение фазы информационного - наибольшего находящегося в заданной полосе - сигнала
*******************************/
int SLL::Find_ff_ph_delta(byte *f_data_array, int min_filter_limit, int max_filter_limit){
	/* В зависимости от характера микрофонной решётки сигнал может быть значительно 
	отличаться от требуемого для библиотеки FFT. Приводим его в надлежащий вид*/ 
	/*char eal_arr[128] = {0} ;
	char* real_arr = &eal_arr[0];
	char mage_arr[128] = {0};
	char* image_arr = &mage_arr[0];
	SLL::Sig_to_sin(&f_data_array[0], &real_arr[0]);
	int hase_arr[64] = {0} ;
	int* phase_arr = &hase_arr[0];
	int mp_arr[64] = {0} ;
	int* amp_arr = &mp_arr[0];*/
	SLL::Sig_to_sin(&f_data_array[0], &real_arr[0]);
	//Вычисляем действительную и мнимую составляющие на заданных частотах
  	fix_fft(real_arr,image_arr,log(size_data_array)/log(2),0);
  	for(int i=0;i<size_data_array/2;i++){
  		//Находи значения амплитуды и фазы сигнала на заданных частотах
    	amp_arr[i] = sqrt(real_arr[i]*real_arr[i]+image_arr[i]*image_arr[i]);
    	//Фаза из радиан в градусы
    	float f = atan2((int)image_arr[i],(int)real_arr[i])*180/PI;
    	phase_arr[i] = (int)f;
    } 
    int max_amp_spec= 0;
    int pr_max_amp_spec = 0;
    int num = 0;
    freq_to_count = 2.0/size_data_array*freq_sampling/2.0; 
    for(int i=1;i<size_data_array/2;i++){
        max_amp_spec = amp_arr[i];
        //Находим информационный сигнал по максимальному значени. амплитудного спектра сигнала
        if (max_amp_spec>=pr_max_amp_spec && i*freq_to_count>min_filter_limit && i*freq_to_count<max_filter_limit){
           	pr_max_amp_spec = max_amp_spec;
      		num = i;
    	}
    }
    //Вычисление шага спектра - чем больше размер выборки, тем более высокая разрешающая способность 
    freq_sig = 2.0*num/size_data_array*freq_sampling/2.0;
    int  phase_sig  = phase_arr[num];
    return phase_sig;
}
/******************************
Преобразование входного сигнала в сигнал более похожий на синус. Производится нормировка
сигнала, перенос его значений из формата byte в формат char, необходимый для FFT.
В случае, если снята только положительная
Возвращает 1, если фукнкция выполнена корректно,
*******************************/
void SLL::Sig_to_sin(byte* bf_data_array, char* cf_data_array){
	//Максимальное значение 255, минимальное 0
    int max_sig_val = 0;
    int min_sig_val = 1000;
    for(int i=0;i<size_data_array;i++){
    	if (bf_data_array[i]>max_sig_val){
       		max_sig_val = bf_data_array[i];
    	}
    	if ((bf_data_array[i]<max_sig_val)){
    		min_sig_val = bf_data_array[i];
    	}
    }
    float aver_plus = (max_sig_val+min_sig_val)/2.0;
    float aver_minus = max_sig_val-min_sig_val;
    float devide_c = 125.0/max_sig_val;
    //Нормировка и преобразование в char сдвинутого синуса
    if (voltage_thres!=0){
    	for(int i=0;i<size_data_array;i++){
    		float ij = 250.0/aver_minus*(bf_data_array[i]-aver_plus);
    		cf_data_array[i] = ij;
    	}
    }
    //Преобразования половины синуса
    else{
    	int ii = 0;
    	while (ii<size_data_array){
    		int jj=0;
    		//Считаем число нулевых отчётов
    		while (bf_data_array[ii+jj]==0 && jj+ii<size_data_array){
    			jj++; 
   			}
   			//Заполняем нулевые отчёты зеркальным отображением остальных отчётов.
   			//Выолняется до того момента, пока первый отчёт число
   			if (ii==0 && jj!=0){
   				for (int i = jj; i>0;i-- ){
   					float ij = devide_c*bf_data_array[2*jj-i];
   					cf_data_array[i+ii-1] = -ij;
   				}
   				ii = jj;
   			}
   			//Заполняем нулевые отчёты зеркальным отображением остальных отчётов
   			else{
   				for (int i = jj; i>0;i-- ){
   					float ij = -125.0*sin(PI*i/jj);
   					cf_data_array[i+ii] = ij;
   				}
   				ii = ii+jj;
   				jj = 0;
   				if (ii<size_data_array){
   					float ij = devide_c*bf_data_array[ii];
   					cf_data_array[ii] =  ij;
   				}
   				ii = ii+jj+1;
   			}
    	}
    }
}
/******************************
Вычисления числа отчётов между сигналами, снятых с микрофонной решётки методом
кросс-корреляции
Принимает массивы с сигналами, снятыми с микрофонов
Возвращает число отчётов между сигналами  
*******************************/
int SLL::Find_cr_co_delta(byte* data_mic_1,byte* data_mic_2, int f_val_thres){
	double sum_delta = 0;
	double prev_sum = 0;
 	double max_delta = 0;
 	double prev_max_delta = 0;
 	int delta = 0;
 	byte num = 0;
 	byte flag = 0;
 	double aver_num = 0;
  	while (data_mic_1[num]>voltage_thres)
    	num++;
  	for (int i = 0; i<num; i++){
  		if (data_mic_1[num]>=voltage_thres){
    		aver_num = aver_num + data_mic_1[i];
  		}
  	}
    aver_num = aver_num/num;
    //Serial.print("aver_num");
    //Serial.print(aver_num);
    //Serial.print("num");
    //Serial.print(num);                                                     
  	for (int i = 0; i<num_max_count*2; i++){
    	for (int j = 0; j<num; j++){
    		//Вычисление кросс корреляции мжду сигналами
    		prev_sum = (double)data_mic_1[j]*data_mic_2[i+j];
    		sum_delta =sum_delta + prev_sum;
    	}
    	//Как критерий выбираем первый максимум функции, больше заданного порога
    	//Serial.print(' ');
        //Serial.print(sum_delta); 
    	if ((sum_delta > abs(aver_num*(f_val_thres+voltage_thres)*num))&&(flag!=1)){
         	flag = 1;
        	if (sum_delta>max_delta){
            	max_delta = sum_delta;
            	flag = 0;
            }
        }
        if ((max_delta>prev_max_delta)&&(flag!=1)){
        	prev_max_delta = max_delta;
        	delta = i;
        }                                         
        sum_delta = 0;
        prev_sum = 0;                                          
    } 
    //Serial.println(" ");
	return delta;  
}
/******************************
Метод, определяющий угловые координаты источника звука в пространстве
Принимает номер микрофона, на котором впервые был зафиксирован информационный сигнал
Возвращает массив из азимутального угла и угла наклона
*******************************/
void SLL::Find_cr_co_angle(byte f_num_first_mic,double delta_1,double delta_2){
	switch (num_mic){
		//Для квадратной микрофонной решётки
    	case 4:
    		int delta = 0;
   			byte mode_angle = 2;
    		int angle_qu = 0;
    		int angle_phi = 0;
    		delta = min(delta_1,delta_2);
    		if (delta==delta_1)
    			mode_angle = 1;
    		delta_2 = max(delta_1,delta_2);
    		delta_1 = delta;
    		//Вычислени еазимутального угла. В роли дельт может выступать любые величины, пропрциональные
    		//Величине временных задержек между сигналами
    		angle_phi = atan2(delta_1,delta_2)*180/PI;
    		angle_phi = round(angle_phi);
    		if (mode_angle==2)
       			angle_phi = angle_phi+90*f_num_first_mic;
    		if (mode_angle==1)
       			angle_phi = 90-angle_phi+90*f_num_first_mic;
       		//Вычисление зенитного угла. В данном  случае num_max_count должна измеряься в тех же величинах
       		// что и delta    
    		angle_qu = round(acos((sqrt(delta_2*delta_2+delta_1*delta_1))/num_max_count)*180/PI); 
    		result_angle[0] = angle_phi;
    		result_angle[1] = angle_qu; 
    		}       
}
/******************************
Метод, определяющий расстояние от микрофонной решётки до источника звука при использование двух 
решёток или сдвиге одной решётки на заданное расстояние 
angle_phi, angle_qu - угловые координаты, вычесленные на второй микрофонной решётке
distanse_be_mic_arr - расстояние между микрофонными решётками. При эом решётки должны быть сдвинуты 
по одной оси в см. 
f_val_thres - величина порога срабатывания алгортма для определения углов
Возвращает массив lenght, содержащий координаты источника звука относительно решётки, на которой выполняется
данная функция в см.
*******************************/
int* SLL::Find_distance(int angle_phi_trans, int angle_qu_trans,  int distanse_be_mic_arr){
	int angle[4] = {0};
    angle[0] = result_angle[0];
    angle[1] = result_angle[1];
    angle[2] = angle_phi_trans;
    angle[3] = angle_qu_trans;
    float Y = distanse_be_mic_arr/(tan(angle[0]*PI/180)-tan(angle[2]*PI/180));
    float X = Y*tan(angle[0]*PI/180);
    float Z = sqrt(X*X+Y*Y)*tan(angle[1]*PI/180);
    lenght[0] = X;
    lenght[1] = Y;
    lenght[2] = Z;
    return lenght;
}
/******************************
Дополнительный метод, возвращает значения задержек между микрофонами
*******************************/
int* SLL::Get_delta(){
	return result_delta;
}
