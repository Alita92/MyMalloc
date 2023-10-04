#include <iostream>
#include <vector>
#include <list>
#include <crtdbg.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <random>
using namespace std;

int Random(int min, int max)
{
    return static_cast<int>(rand() % (max + 1UL - min)) + min;
}

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

        Node node;
        node._is_using = false;
        node._size = _size;
        node._start_address = reinterpret_cast<char*>(_memory_block);
        _memory_start_list.push_back(node);
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
    void* Malloc(size_t bytes)
    {
        if (_used_memory + bytes <= _size)
        {
            // 리스트를 순회하며 자리 찾기
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

                        _used_memory += bytes;
                        printf("할당 완료... index : %d , address : %p, size : %zu \n", index, iter->_start_address, bytes);
                        return iter->_start_address;

                    }
                    else if (iter_size > new_node_size) // 해당 반복자 노드의 크기가 새로운 노드보다 크다면
                    {
                        // 신규 노드 준비
                        Node new_node;
                        new_node._size = bytes;
                        new_node._start_address = iter->_start_address;
                        new_node._is_using = true;

                        // 기존 비활성화 노드 크기 조절
                        iter->_size -= bytes;
                        iter->_start_address += bytes;

                        _memory_start_list.insert(iter, new_node);

                        _used_memory += bytes;
                        printf("할당 완료... index : %d , address : %p, size : %zu \n", index, new_node._start_address, bytes);
                        return iter->_start_address;
                    }
                }

                ++index;
                continue;
            }
        }
        else
        {
            printf("힙 공간이 부족합니다.\n");
            return nullptr;
        }

        printf("힙 공간은 충분하지만 파편화가 되어 할당이 불가합니다.\n");
        return nullptr;
    }

    void Free(int index)
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
                _used_memory -= iter->_size;

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
               
               
                {
                    auto next_iter = next(iter, 1);

                    if (next_iter != _memory_start_list.end())
                    {
                        if (next_iter->_is_using == false)
                        {
                            char* next_address = next_iter->_start_address;
                            size_t next_size = next_iter->_size;

                            iter->_size += next_size;

                            _memory_start_list.erase(next_iter);
                        }
                    }
                }
             
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

        printf("전체 힙 메모리 : %zu bytes / 사용 중인 메모리 : %zu bytes \n", _size, _used_memory);

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

private:
    void* _memory_block;
    size_t _size;
    size_t _used_memory;
    list<Node> _memory_start_list;

public:
    size_t Size() { return _size; }
    size_t GetUsedMemory() { return _used_memory; }
};

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
            cin.clear();
            return 0;
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
        printf("Q : 종료 / Z : 메모리 할당 / X : 메모리 해제 / C : 현재 힙 상태 / V : 메모리 자동 할당(디버깅)\n");
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
        {
            printf("Z키 : 메모리 할당\n");
            printf("할당하려는 크기를 입력해주세요.\n");
            size_t bytes = 0;
            cin >> bytes;

            if (bytes <= 0)
            {
                printf("잘못된 값이 입력되었습니다.\n");
                break;
            }

            memory_manager.Malloc(bytes);
        }
            break;
        case 'x':
        case 'X':
        {
            printf("X키 : 메모리 해제\n");
            memory_manager.Show();
            printf("해제할 메모리의 인덱스를 입력해주세요! \n");

            size_t index = 0;
            cin >> index;

            if (index < 0)
            {
                printf("잘못된 인덱스입니다. 메인 메뉴로 돌아갑니다.\n");
                break;
            }

            memory_manager.Free(static_cast<int>(index));
        }
            break;
        case 'c':
        case 'C':
            printf("C키 : 메모리 상태\n");
            memory_manager.Show();
            break;
        case 'v':
        case 'V':
        {
            printf("V키 : 메모리 랜덤 자동 할당\n");

            size_t size = memory_manager.Size();
            size_t used_memory = memory_manager.GetUsedMemory();

            while (used_memory <= size)
            {
                int random_size = Random(1, 8);

                if (used_memory + random_size > size)
                {
                    break;
                }

                auto result = memory_manager.Malloc(random_size);

                if (result == nullptr)
                {
                    break;
                }
            }
        }
        default:
            break;
        }
    }
}