# 코드 작성 규칙

이 프로젝트는 줄바꿈을 충분히 사용하는 Allman 스타일을 따른다.
자동 정렬 기준은 루트의 `.clang-format`이며, clang-format 22 기준으로 작성했다.

- 들여쓰기는 탭 대신 공백 4칸을 사용한다.
- 함수, 클래스, 구조체, 열거형, 람다, 제어문의 여는 중괄호는 다음 줄에 둔다.
- 한 줄짜리 `if`, `else`, `for`, `while`도 중괄호로 감싼다.
- 한 줄에 여러 실행문을 나열하지 않는다.
- 짧은 접근자와 `return`도 함수 본문을 여러 줄로 작성한다.
- 함수·타입 정의 사이와 초기화, 처리, 저장 등 논리 단위 사이에 빈 줄 1개를 둔다.
- 함수 호출이 길어지면 인수를 줄별로 나눈다. 목표 줄 길이는 100자다.
- 포인터와 참조는 `Type* name`, `const Type& name`으로 쓴다.
- include 순서와 기존 이름은 유지한다. 특히 `stdafx.h`의 위치를 바꾸지 않는다.
- 문자열 내용과 기존 파일 인코딩을 유지한다. 한글 소스에 적용된 `/utf-8` 설정도 유지한다.
- GLSL도 동일한 중괄호·들여쓰기 원칙을 사용한다. 내장 셰이더는 여러 줄 raw 문자열로 작성한다.

```cpp
bool CanInteract(const Npc& npc)
{
    if (!npc.IsAvailable())
    {
        return false;
    }

    return npc.IsNearby();
}
```

직접 관리하는 `SimpleGame`의 C++ 소스·헤더와 `Shaders`만 정리한다.
외부 라이브러리인 `Dependencies`, 빌드 산출물과 저장 파일은 대상이 아니다.
자동 정렬에서는 문자열·주석의 재배치나 include 정렬을 사용하지 않는다.
GLSL raw 문자열 내부의 줄바꿈과 논리 단위의 빈 줄은 작성 시 직접 맞춘다.

PowerShell에서 로컬 clang-format으로 정렬하는 예:

```powershell
$sourceFiles = rg --files SimpleGame -g '*.cpp' -g '*.h' -g '!**/Dependencies/**' -g '!**/Debug/**' -g '!**/Release/**'
clang-format --style=file -i $sourceFiles
```

빌드·실행·결과 검증은 사용자가 별도로 요청할 때만 진행한다.
