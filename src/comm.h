#pragma once

#include "DataProcessor.h"

namespace Common {

    struct ICommandNotifier
    {
        virtual void OnCommand() = 0;
    };

    enum class CommandType
    {
        kNull,
        kUpdateCounter,
    };

    struct Command
    {
        Command(CommandType type_) 
            : type(type_) 
        {}

        CommandType type;
    };

    struct Command_UpdateCounter : Command
    {
        Command_UpdateCounter() : Command(CommandType::kUpdateCounter) {}

        int nRead;
        int nWritten;
        int nQueued;
    };

    class CommandQueue
    {
    public:
        void set_notifier(ICommandNotifier* p) {
            _notifier = p;
        }

        void notify() {
            _notifier->OnCommand();
        }

        bool empty() {
            c_lock_guard _guard(_lock);
            return _commands.empty();
        }

        void clear() {
            c_lock_guard _guard(_lock);

            for(auto& p : _commands)
                delete p;

            _commands.clear();
        }

        void push_back(Command* cmd) {
            c_lock_guard _guard(_lock);
            _commands.push_back(cmd);
            notify();
        }

        void push_front(Command* cmd) {
            c_lock_guard _guard(_lock);
            _commands.push_front(cmd);
            notify();
        }

        Command* pop_back() {
            c_lock_guard _guard(_lock);
            auto p = _commands.back();
            _commands.pop_back();
            return p;
        }

        Command* pop_front() {
            c_lock_guard _guard(_lock);
            auto p = _commands.front();
            _commands.pop_front();
            return p;
        }

        Command* try_pop_front() {
            c_lock_guard _guard(_lock);
            Command* pCmd;
            if(!_commands.empty()) {
                pCmd = _commands.front();
                _commands.pop_front();
            }
            else {
                pCmd = nullptr;
            }
            return pCmd;
        }

    private:
        c_critical_locker       _lock;
        std::list<Command*>     _commands;
        ICommandNotifier*       _notifier;
    };
    
	//////////////////////////////////////////////////////////////////////////
	// ���¶��� �������ݷ�װ�����ͽṹ

	// Ĭ�Ϸ��ͻ�������С, �����˴�С���Զ����ڴ����
	const int csdp_def_size = 1024;
	enum csdp_type{
		csdp_local,		// ���ذ�, ����Ҫ�ͷ�
		csdp_alloc,		// �����, ������������Ĭ�ϻ��������ڲ�����������ʱ������
		csdp_exit,		
	};

#pragma warning(push)
#pragma warning(disable:4200)	// nonstandard extension used : zero-sized array in struct/union
#pragma pack(push,1)
	// �����������ݰ�, ������������
	struct c_send_data_packet{
		csdp_type		type;			// ������
		list_s			_list_entry;	// �������ӵ����Ͷ���
		bool			used;			// �Ƿ��ѱ�ʹ��
		int				cb;				// ���ݰ����ݳ���
		unsigned char	data[0];
	};

	// ��չ�������ݰ�, ��һ�� csdp_def_size ��С�Ļ�����
	struct c_send_data_packet_extended{
		csdp_type		type;			// ������
		list_s			_list_entry;	// �������ӵ����Ͷ���
		bool			used;			// �Ƿ��ѱ�ʹ��
		int				cb;				// ���ݰ����ݳ���
		unsigned char	data[csdp_def_size];
	};
#pragma pack(pop)
#pragma warning(pop)

	// �������ݰ�������, ���������͵����ݰ��ųɶ���
	// ���������ᱻ����߳�ͬʱ����
	class c_data_packet_manager
	{
	public:
		c_data_packet_manager();
		~c_data_packet_manager();
		void					empty();
		c_send_data_packet*		alloc(int size);						// ͨ���˺�����ȡһ����������ָ����С���ݵİ�
		void					release(c_send_data_packet* psdp);		// ����һ����
		void					put(c_send_data_packet* psdp);			// ���Ͷ���β����һ���µ����ݰ�
		void					put_front(c_send_data_packet* psdp);	// ����һ���������ݰ���������, ���ȴ���
		c_send_data_packet*		get();									// ����ȡ�ߵ��ô˽ӿ�ȡ�����ݰ�, û�а�ʱ�ᱻ����
		c_send_data_packet*		query_head();
		HANDLE					get_event() const { return _hEvent; }

	private:
		c_send_data_packet_extended	_data[100];	// Ԥ����ı��ذ��ĸ���
		c_critical_locker			_lock;		// ���߳���
		HANDLE						_hEvent;	// ����get()�ɲ�������, ��Ϊ�����ط�Ҫput()!
		list_s						_list;		// �������ݰ�����
	};

	//////////////////////////////////////////////////////////////////////////
	// ���¹���������ص�һЩ����, ��: �������б� ...

	// ���ڶ���
	class t_com_item
	{
	public:
		t_com_item(int i,const char* s){_s = s; _i=i;}

		// �����ַ�������: ����: ��У��λ
		std::string get_s() const {return _s;}
		// ������������ : ����: NOPARITY(��)
		int get_i() const {return _i;}

	protected:
		std::string _s;
		int _i;
	};

	// ˢ�´��ڶ����б�ʱ��Ҫ�õ��Ļص���������
	typedef void t_list_callback(void* ud, const t_com_item* t);

	// ���ڶ���ˢ��ʱ�Ļص����ͽӿ�
	class i_com_list
	{
	public:
		virtual void callback(t_list_callback* cb, void* ud) = 0;
	};

	// ���ڶ�������: ���� ����ϵͳ���еĴ����б�
	template<class T>
	class t_com_list : public i_com_list
	{
	public:
		void empty() {_list.clear();}
		const T& add(T t) { _list.push_back(t); return _list[_list.size() - 1]; }
		int size() {return _list.size();}
		const T& operator[](int i) {return _list[i];}

		// ���¶����б�, �������ϵͳ�����б�
		virtual i_com_list* update_list(){return this;}

		virtual operator i_com_list*() {return static_cast<i_com_list*>(this);}
		virtual void callback(t_list_callback* cb, void* ud)
		{
			for(int i=0,c=_list.size(); i<c; i++){
				cb(ud, &_list[i]);
			}
		}

	protected:
		std::vector<T> _list;
	};

	// ���ڶ˿��б�, �̳е�ԭ����: �˿���һ����ν�� "�Ѻ���"
	// ���糣����: Prolific USB-to-Serial Comm Port
	// ���µ������б��ؼ���ʱ��Ҫ��������һ��
	class c_comport : public t_com_item
	{
	public:
		c_comport(int id,const char* s)
			: t_com_item(id, s)
		{}

		std::string get_id_and_name() const;
	};

	// ���ڶ˿�����: Ҫ��ϵͳȡ���б�, ������д
	class c_comport_list : public t_com_list<c_comport>
	{
	public:
		virtual i_com_list* update_list();
	};

	// ���ڲ����ʿ����ⲿ�ֶ�����, ���Զ��һ����Ա
	class c_baudrate : public t_com_item
	{
	public:
		c_baudrate(int id, const char* s, bool inner)
			: t_com_item(id, s)
			, _inner(inner)
		{}

		bool is_added_by_user() const { return !_inner; }
	protected:
		bool _inner;
	};

	// �����¼��������ӿ�
	class i_com_event_listener
	{
	public:
		virtual void do_event(DWORD evt) = 0;
	};

	// do_event�Ĺ���һ����Ҫ��ʱ�䲻����, ����д�˸������¼��ļ�����
	class c_event_event_listener : public i_com_event_listener
	{
	public:
		operator i_com_event_listener*() {
			return this;
		}
		virtual void do_event(DWORD evt) override
		{
			event = evt;
			::SetEvent(hEvent);
		}

	public:
		c_event_event_listener()
		{
			hEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		}
		~c_event_event_listener()
		{
			::CloseHandle(hEvent);
		}

		void reset()
		{
			::ResetEvent(hEvent);
		}


	public:
		DWORD	event;
		HANDLE	hEvent;
	};

	// �����¼��������ӿ� ������
	class c_com_event_listener
	{
		struct item{
			item(i_com_event_listener* p, DWORD _mask)
				: listener(p)
				, mask(_mask)
			{}

			i_com_event_listener* listener;
			DWORD mask;
		};
	public:
		void call_listeners(DWORD dwEvt){
			_lock.lock();
			for (auto& item : _listeners){
				if (dwEvt & item.mask){
					item.listener->do_event(dwEvt);
				}
			}
			_lock.unlock();
		}
		void add_listener(i_com_event_listener* pcel, DWORD mask){
			_lock.lock();
			_listeners.push_back(item(pcel, mask));
			_lock.unlock();
		}
		void remove_listener(i_com_event_listener* pcel){
			_lock.lock();
			for (auto it = _listeners.begin(); it != _listeners.end(); it++){
				if (it->listener == pcel){
					_listeners.erase(it);
					break;
				}
			}
			_lock.unlock();
		}

	protected:
		c_critical_locker	_lock;
		std::vector<item>	_listeners;
	};

	// ������
	class CComm
	{
        CommandQueue _commands;
    public:
        void set_notifier(ICommandNotifier* notifier) {
            _commands.set_notifier(notifier);
        }

        Command* get_command() {
            return _commands.try_pop_front();
        }

	// �������ݰ�����
	private:
		c_send_data_packet*		get_packet()	{ return _send_data.get(); }
	public:
		bool					put_packet(c_send_data_packet* psdp, bool bfront=false, bool bsilent = false){
			if (is_opened()){
				if (bfront)
					_send_data.put_front(psdp);
				else
					_send_data.put(psdp);
				
				switch (psdp->type)
				{
				case csdp_type::csdp_alloc:
				case csdp_type::csdp_local:
                    update_counter(0, 0, psdp->cb);
					break;
				}
				return true;
			}
			else{
                // TODO
				//if (!bsilent)
				//	_notifier->msgbox(MB_ICONERROR, NULL, "����δ��!");
				release_packet(psdp);
				return false;
			}
		}
		c_send_data_packet*		alloc_packet(int size) { return _send_data.alloc(size); }
		void					release_packet(c_send_data_packet* psdp) { _send_data.release(psdp); }
		void					empty_packet_list() { _send_data.empty(); }
	private:	
		c_data_packet_manager	_send_data;

	// ������
	public:
        void get_counter(int* pRead, int* pWritten, int* pQueued);
        void reset_counter(bool r = true, bool w = true, bool q = true);
    private:
        void update_counter(int nRead, int nWritten, int nQueued);
        volatile long _nRead, _nWritten, _nQueued;

	// �¼�������
	private:
		c_com_event_listener	_event_listener;

	// ���ݽ�����
	public:
		void add_data_receiver(i_data_receiver* receiver);
		void remove_data_receiver(i_data_receiver* receiver);
		void call_data_receivers(const unsigned char* ba, int cb);
	private:
		c_ptr_array<i_data_receiver>	_data_receivers;
		c_critical_locker				_data_receiver_lock;

	// �ڲ������߳�
	private:
		bool _begin_threads();
		bool _end_threads();

		class c_overlapped : public OVERLAPPED
		{
		public:
			c_overlapped(bool manual, bool sigaled)
			{
				Internal = 0;
				InternalHigh = 0;
				Offset = 0;
				OffsetHigh = 0;
				hEvent = ::CreateEvent(nullptr, manual, sigaled ? TRUE : FALSE, nullptr);
			}
			~c_overlapped()
			{
				::CloseHandle(hEvent);
			}
		};

		struct thread_helper_context
		{
			CComm* that;
			enum class e_which{
				kEvent,
				kRead,
				kWrite,
			};
			e_which which;
		};
		struct thread_state{
			HANDLE hThread;
			HANDLE hEventToBegin;
			HANDLE hEventToExit;
		};
		unsigned int thread_event();
		unsigned int thread_read();
		unsigned int thread_write();
		static unsigned int __stdcall thread_helper(void* pv);

		thread_state	_thread_read;
		thread_state	_thread_write;
		thread_state	_thread_event;

	private:


	// �������ýṹ��
	private:
		//COMMPROP			_commprop;
		//COMMCONFIG			_commconfig;
		COMMTIMEOUTS		_timeouts;
		DCB					_dcb;
	// ��������(���ⲿ����)
	public:
		struct s_setting_comm{
			DWORD	baud_rate;
			BYTE	parity;
			BYTE	stopbit;
			BYTE	databit;
		};
		bool setting_comm(s_setting_comm* pssc);
        DCB& get_dcb() {
            return _dcb;
        }


	// ���ڶ����б�
	public:
		c_comport_list*			comports()	{ return &_comport_list; }
		t_com_list<c_baudrate>*	baudrates()	{ return &_baudrate_list; }
		t_com_list<t_com_item>*	parities()	{ return &_parity_list; }
		t_com_list<t_com_item>*	stopbits()	{ return &_stopbit_list; }
		t_com_list<t_com_item>*	databits()	{ return &_databit_list; }
	private:
		c_comport_list			_comport_list;
		t_com_list<c_baudrate>	_baudrate_list;
		t_com_list<t_com_item>	_parity_list;
		t_com_list<t_com_item>	_stopbit_list;
		t_com_list<t_com_item>	_databit_list;

	// �ⲿ��ز����ӿ�
	private:
		HANDLE		_hComPort;
	public:
		bool		open(int com_id);
		bool		close();
		HANDLE		get_handle() { return _hComPort; }
		bool		is_opened() { 
			return !!_hComPort; 
		}
		bool		begin_threads();
		bool		end_threads();

	public:
		CComm();
		~CComm();
	};
}