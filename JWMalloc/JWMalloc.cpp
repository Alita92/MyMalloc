#include <iostream>
#include <vector>
#include <list>
#include <crtdbg.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

struct Node
{
    char* _start_address = nullptr;
    size_t _size = 0;
    bool _is_using = false;
};

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

        if (_memory_start_list.empty() == false)
        {
            int leak_size = 0;
            for (auto node : _memory_start_list)
            {
                int bytes = (int)node._size;

                if (node._is_using == false)
                {
                    continue;
                }

                leak_size += bytes;

                printf("해제되지 않은 메모리 : % p  .... % d bytes. \n", node._start_address, bytes);
            }

            printf("%d bytes 만큼의 릭이 감지되었습니다. \n", leak_size);
        }
        _memory_start_list.clear();

        free(_memory_block);
    }

    // 메모리 할당 함수
    void* SetMalloc(size_t bytes)
    {
        if (_used_memory + bytes <= _size)
        {
            // 리스트를 순회하며 자리 찾기
            if (_memory_start_list.empty())
            {
                Node node;
                node._start_address = reinterpret_cast<char*>(_memory_block);
                node._size = bytes;
                node._is_using = true;
                _memory_start_list.push_back(node);
                _used_memory += bytes;
                printf("할당 완료... index : 0 , address : %p, size : %zu \n", node._start_address, bytes);
                return _memory_block;
            }
            else
            {
                list<Node>::iterator iter = _memory_start_list.begin();
                size_t iter_count_size = 0;
                int index = 0;

                for (; iter != _memory_start_list.end(); ++iter)
                {
                    size_t iter_size = iter->_size;
                    size_t new_node_size = bytes;

                    iter_count_size += iter_size;

                    if (iter->_is_using == false)
                    {
                        if (iter_size == new_node_size)
                        {
                            // 해당 반복자와의 사이즈가 같고 반복자 위치의 메모리가 사용 중이 아니라면
                            // 이 포인터를 사용 중으로 바꾸고 리턴해준다
                            iter->_is_using = true;
                            printf("할당 완료... index : %d , address : %p, size : %zu \n", index, iter->_start_address, bytes);
                            return iter->_start_address;
                        }
                        else if (iter_size > new_node_size) // 해당 반복자 노드의 크기가 새로운 노드보다 크다면
                        {
                            // 노드 두 개로 세분화한다
                            iter->_size = bytes;

                            size_t size_left_over = iter_size - new_node_size;
                            Node node_left_over;
                            node_left_over._size = size_left_over;
                            node_left_over._start_address = iter->_start_address + iter->_size;
                            node_left_over._is_using = false;

                            // 리스트 insert 가 iter 다음이 아닌 이전에 삽입하는지 확인해볼 것...
                           // _memory_start_list.insert(iter, node_left_over);
                            _memory_start_list.insert(next(iter, 1), node_left_over);

                            iter->_is_using = true;
                            printf("할당 완료... index : %d , address : %p, size : %zu \n", index, iter->_start_address, bytes);
                            return iter->_start_address;
                        }
                    }

                    ++index;
                    continue;
                }

                // 여기까지 오면 빈 자리는 있지만 사이즈가 맞지 않아 할당을 못 하는 상황입니다.
                    // 마지막 사용 중 노드의 뒤에 충분한 사이즈가 있다면 메모리 할당을 시도합니다.
                if (bytes <= _size - iter_count_size)
                {
                    Node node;
                    node._start_address = _memory_start_list.back()._start_address + _memory_start_list.back()._size;
                    node._size = bytes;
                    node._is_using = true;
                    _memory_start_list.push_back(node);

                    _used_memory += bytes;

                    printf("할당 완료... index : %d , address : %p, size : %zu \n", _memory_start_list.size() - 1, node._start_address, bytes);
                    return _memory_start_list.back()._start_address;
                }
                else
                {
                    printf("힙 공간은 충분하지만 파편화가 되어 할당할 수 없습니다.\n");
                    return nullptr;
                }
            }
        }
        else
        {
            printf("힙 공간이 부족합니다.\n");
            return nullptr;
        }

        return nullptr;
    }

    // 메모리 해제 함수

    // 인덱스로 해제
    void SetFree(int index)
    {
        list<Node>::iterator iter = _memory_start_list.begin();
        
        int cur_index = 0;

        if (index > _memory_start_list.size())
        {
            printf("메모리가 정상적으로 free 되지 않았습니다. 인덱스 범위가 초과되었습니다.\n");
            return;
        }
        else if (index < 0)
        {
            printf("메모리 인덱스는 0 미만의 음수가 될 수 없습니다.\n");
            return;
        }

        // free 가 되었는데 주변에 사용하지 않는 빈 노드가 있다면 merge 진행해야 할 것.
        for (; iter != _memory_start_list.end(); ++iter)
        {
            if (cur_index == index)
            {
                if (iter->_is_using == false)
                {
                    printf("이미 해제된 메모리입니다.\n");
                    return;
                }

                iter->_is_using = false;

                // 이전과 다음 노드가 비활성화 상태라면 해당 노드를 erase 해 주고 사이즈와 주소를 통합시킨다
                if (index != 0)
                {
                    auto prev_iter = prev(iter, 1);

                    if (prev_iter != _memory_start_list.end())
                    {
                        if (prev_iter->_is_using == false)
                        {
                            char* prev_address = prev_iter->_start_address;
                            size_t prev_size = prev_iter->_size;

                            iter->_start_address = prev_address;
                            iter->_size += prev_size;

                            _memory_start_list.erase(prev_iter);
                        }
                    }
                }
               
                if (index < _memory_start_list.size())
                {
                    auto next_iter = next(iter, 1);

                    if (next_iter != _memory_start_list.end())
                    {
                        if (next_iter->_is_using == false)
                        {
                            char* next_address = next_iter->_start_address;
                            size_t next_size = next_iter->_size;

                            iter->_size += next_size;       // 다음 노드가 통합 가능 노드라면 주소값은 그대로 두고 사이즈만 더해줍니다

                            _memory_start_list.erase(next_iter);
                        }
                    }
                }

                if (iter->_start_address == _memory_start_list.back()._start_address) // 맨 뒤일때
                {
                    _memory_start_list.erase(iter);
                }
             
                _used_memory -= iter->_size;

                Show();
                printf("메모리 free 완료.\n");
                return;
            }

            ++cur_index;
        }

        printf("메모리가 정상적으로 free 되지 않았습니다. 잘못된 주소입니다.\n");
    }


    void Show()
    {
        printf("------------------SHOW-----------------------\n");

        printf("전체 힙 메모리 : %zu bytes\n", _size);

        int index = 0;

        for (auto node : _memory_start_list)
        {
            char* ptr = node._start_address;
            int size = static_cast<int>(node._size);

            if (node._is_using)
            {
                printf("Index %d : %p / Size : %d bytes\n", index, ptr, size);
            }
            else
            {
                printf("Index %d : %p / Size : %d bytes (Deallocated)\n", index, ptr, size);
            }

            ++index;
        }

        printf("------------------SHOW END-----------------------\n");
    }
    
    void SetMemoryMenu()
    {
        printf("메모리의 자료형을 선택해주세요. \n1번 : char\n2번 : int\n그 외 : 돌아가기\n");

        char key;
        key = _getch();

        int int_key = key - '0';

        switch (int_key)
        {
        case 1:
        {
            (char*)(SetMalloc(sizeof(char)));
        }
        break;
        case 2:
        {
            (int*)(SetMalloc(sizeof(int)));
        }
        break;
        default:
            printf("메모리 할당을 취소합니다. \n");
            break;
        }
    }

    void FreeMemoryMenu()
    {
        Show();
        printf("해제할 메모리의 인덱스를 입력해주세요! \nQ : 취소\n");

        char key;
        key = _getch();

        if (key == 'q' || key == 'Q')
        {
            printf("메모리 해제 취소, 메인 메뉴로 돌아갑니다.\n");
            return;
        }

        int memory_index = key - '0';
        SetFree(memory_index);
        return;
    }

private:
    void* _memory_block;
    size_t _size;
    size_t _used_memory;

   // vector<pair<char*,int>> _memory_start_point_list;
    // 힙메모리에 할당된 메모리들의 주소값을 저장
    // first : 주소  second : 사용 중인 메모리 바이트
    list<Node> _memory_start_list;
};

// 사용자 정의 노드를 사용한 list 를 만들면...
// 노드 안에 가져야 할 거?
// 헤드 주소값... char*
// 할당된 크기... size_t...
// 더 없나?


int main()
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    bool is_making_heap = true;

    printf("프로세스를 시작합니다.\n 최대 힙 메모리의 바이트 크기를 정해주세요.\n");

    size_t size = 0;
    while (is_making_heap)
    {
        cin >> size;

        if (size <= 0)
        {
            printf("최대 힙 메모리는 0 이하가 될 수 없습니다.\n 최대 힙 메모리의 바이트 크기를 정해주세요.\n");
        }
        else
        {
            is_making_heap = false;
            break;
        }
    }

    JWMalloc memory_manager(size); // 힙 메모리 생성
    printf("현재 힙 사이즈 : %zu bytes \n", size);

    bool is_run = true;
    
    while (is_run)
    {
        printf("Q : 종료 / Z : 메모리 할당 / X : 메모리 해제 / C : 현재 힙 상태\n");
        char key;
        key = _getch();

        switch (key)
        {
        case 'q':
        case 'Q':
            // 프로세스 종료
            printf("프로세스를 종료합니다.\n");
            is_run = false;
            break;
        case 'z':
        case 'Z':
            printf("z키 : 메모리 할당\n");
            memory_manager.SetMemoryMenu();
            break;
        case 'x':
        case 'X':
            printf("z키 : 메모리 해제\n");
            memory_manager.FreeMemoryMenu();
            break;
        case 'c':
        case 'C':
            memory_manager.Show();
            break;
        default:
            break;
        }
    }
}