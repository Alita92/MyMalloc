#include <cstdlib>
#include <iostream>
#include <vector>
#include <unordered_map>
using namespace std;

class JWMalloc
{
public:
    JWMalloc(size_t size)
    {
        _memory_block = malloc(size);
        _size = size;
        _used_memory = 0;
    }

    ~JWMalloc()
    {
        printf("프로세스 종료!\n");

        if (_memory_start_point_list.empty() == false)
        {
            int leak_size = 0;
            for (size_t index = 0; index < _memory_start_point_list.size(); ++index)
            {
                int bytes = _memory_start_point_list[index].second;

                if (bytes < 0)
                {
                    continue;
                }

                leak_size += bytes;

                printf("해제되지 않은 메모리 : % p  .... % d bytes. \n", _memory_start_point_list[index].first, _memory_start_point_list[index].second);
            }

            printf("%d bytes 만큼의 릭이 감지되었습니다. \n", leak_size);
        }
        _memory_start_point_list.clear();

        free(_memory_block);
    }

    // 메모리 할당 함수
    void* SetMalloc(size_t bytes)
    {
        if (_used_memory + bytes <= _size)
        {
            void* allocation = reinterpret_cast<char*>(_memory_block) + _used_memory;
            _used_memory += bytes;

            _memory_start_point_list.push_back(pair<char*, int>(reinterpret_cast<char*>(allocation), (int)bytes));

            return allocation;
        }
        else
        {
            printf("힙 공간이 부족합니다.");
            return nullptr;
        }
    }

    // 메모리 해제 함수
    void SetFree(void* ptr)
    {
        for (size_t index = 0; index < _memory_start_point_list.size(); ++index)
        {
            if (_memory_start_point_list[index].first == ptr)
            {
                if (_memory_start_point_list[index] == _memory_start_point_list.back())
                {
                    _used_memory -= _memory_start_point_list[index].second;
                    _memory_start_point_list.pop_back();
     
                    // 마지막 요소면 메모리 영역 자체를 지워 준다
                }
                else
                {
                    _memory_start_point_list[index].second *= -1;
                    // 마지막 요소가 아닐 시에는 음수로 바꿔 준다
                }
            }
        }
    }

    void Show()
    {
        printf("------------------SHOW-----------------------\n");

        printf("전체 힙 메모리 : %d bytes\n", _size);

        for (size_t index = 0; index < _memory_start_point_list.size(); ++index)
        {
            char* ptr = _memory_start_point_list[index].first;
            int size = _memory_start_point_list[index].second;

            if (size < 0)
            {
               
                size *= -1;
                printf("단편화된 메모리 감지됨... 메모리 : %p, 크기: %d bytes\n", ptr, size);
            }
            else
            {
                printf("할당된 메모리 : %p, 크기 : %d bytes\n", ptr, size);
            }
        }


        printf("------------------SHOW END-----------------------\n");
    }

private:
    void* _memory_block;
    size_t _size;
    size_t _used_memory;

    vector<pair<char*,int>> _memory_start_point_list;
    // 힙메모리에 할당된 메모리들의 주소값을 저장
    // first : 주소  second : 사용 중인 메모리 바이트
};


int main()
{
    JWMalloc memory_manager(1024); // 힙 메모리 생성

    int* a = (int*)(memory_manager.SetMalloc(sizeof(int))); 
    char* b = (char*)(memory_manager.SetMalloc(sizeof(char)));
    int* c = (int*)(memory_manager.SetMalloc(sizeof(int)));
    // 3번의 동적 할당
    // [a][a][a][a][b][c][c][c][c]

    memory_manager.SetFree(b); 
    // 중간 메모리 해제
    // [a][a][a][a][해제][c][c][c][c]
    // 단편화 발생

    memory_manager.Show();

    int* d = (int*)(memory_manager.SetMalloc(sizeof(int)));
    // 추가 동적 할당
    // [a][a][a][a][해제][c][c][c][c][d][d][d][d]
    
    memory_manager.SetFree(d);
    // 마지막 메모리 해제
    // [a][a][a][a][해제][c][c][c][c]
    // 단편화 발생하지 않음

    memory_manager.Show();

}