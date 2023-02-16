/* Editor.cpp : 文本编辑器*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define T_DrawBox(x, y, w, h, c) Text_Draw_Box((y), (x), (h) + y, (w) + x, (c))
#define MAX_LINE 24
#define PORT 0x3f8  // COM1
extern "C" void io_out8(int port, int data);
extern "C" int io_in8(int port);
int is_transmit_empty() {
  return io_in8(PORT + 5) & 0x20;
}

void write_serial(char a) {
  while (is_transmit_empty() == 0)
    ;

  io_out8(PORT, a);
}
void kprint(char* str) {
  for (int i = 0; i < strlen(str); i++) {
    write_serial(str[i]);
  }
}
void printk(char* str, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, str);
  vsprintf(buf, str, ap);
  va_end(ap);
  kprint(buf);
}
struct Camera {
  int y;  // 摄像机高度
  int curser_pos_x, curser_pos_y;
  int index;  //光标指向位置的index
  char* buffer;
  int array_len;
  int len;
};
struct Char {
  int index;  // 在buffer中的index
  unsigned char ch;
};
struct Line {
  int line_flag;  // 是不是回车敲出的行的结尾
  // char line[80];
  Char line[80];
  int len;
  int start_index;  // 行首索引
};
void putSpace(int x, int y, int w, int h) {
  goto_xy(x, y);
  for (int i = 0; i < h; i++) {
    goto_xy(x, y + i);
    for (int j = 0; j < w; j++) {
      putch(' ');
    }
  }
}
void setState(char* msg) {
  putSpace(0, 24, 80, 1);
  goto_xy(0, 24);
  print(msg);
  T_DrawBox(0, 24, 80, 1, 0x70);
}
void insert_char(char* str, int pos, char ch, Camera* c) {
  if (c->len + 1 > c->array_len) {
    str = (char*)realloc(c->buffer, c->array_len + 100);
    c->buffer = str;
    c->array_len += 100;
  }
  int i;
  for (i = c->len; i >= pos; i--) {
    str[i + 1] = str[i];
  }
  str[pos] = ch;
  c->len++;
}
void insert_str(char* str, int pos, Camera* c) {
  for (int i = 0; i < c->len; i++) {
    insert_char(c->buffer, pos++, str[i], c);
  }
}
void delete_char(char* str, int pos, Camera* c) {
  int i;
  int l = c->len;
  if (l == 0) {
    return;
  }
  for (i = pos; i < c->len; i++) {
    str[i] = str[i + 1];
  }
  str[l - 1] = 0;
  c->len--;
}
int get_index_of_nth_last_line(int n, char* buf, int pos, int len) {
  int line_count = 0;
  int i = pos - 1;
  int start_of_line = -1;

  // 跳过末尾的换行符
  if (buf[i] == '\n') {
    i--;
  }

  // 倒序扫描找到第n个行的起始位置
  while (i >= 0) {
    if (buf[i] == '\n') {
      line_count++;

      // 找到第n个行的起始位置
      if (line_count == n) {
        start_of_line = i + 1;
        break;
      }
    }

    // 处理行内超过80个字符的情况
    if (i > 0 && i - 1 < start_of_line && (i - start_of_line + 1) % 80 == 0 &&
        buf[i - 1] != '\n') {
      line_count++;
      start_of_line = i;
    }
    i--;
  }

  // 如果找到了第n个行的起始位置，返回该行的起始位置；F否则返回buf的起始位置
  if (start_of_line == -1) {
    return 0;
  } else {
    return start_of_line;
  }
}
int get_index_of_nth_next_line(int n, char* buf, int pos, int len) {
  int i = 0;
  int j = 0;
  for (; pos < len; pos++) {
    if (i == n) {
      return pos;
    }
    if (buf[pos] == '\n' || j == 80) {
      i++;
      j = 0;
    } else {
      j++;
    }
  }
}
class parse {
 public:
  parse(Camera* c) {
    camera = c;
    clean();
    ny = 0;
    nidx = 0;
  }
  void Set() {
    // 根据camera的y值
    clean();
    int l = 0;
    int sc = 0;
    int f = 0;
    int nl = 0;
    int len = 0;
    int sl = 0;
    int i;
    if (ny == camera->y) {
      // printk("Default\n");
      i = nidx;
      l = ny;
    } else if (ny > camera->y) {
      // printk(">\n");
      nidx = get_index_of_nth_last_line(ny - camera->y, camera->buffer, nidx,
                                        camera->len);
      i = nidx;
      l = camera->y;
      ny = l;
    } else {
      i = get_index_of_nth_next_line(camera->y - ny, camera->buffer, nidx,
                                     camera->len);
      nidx = i;
      ny = camera->y;
    }
    for (; i < camera->len && nl < MAX_LINE; i++) {
      // printk("%c", camera->buffer[i] == '\n' ? 'n' : camera->buffer[i]);
      //  printf("OK1\n");
      if (sc == 0) {
        this->l[nl].start_index = i;
        if (nl != 0) {
        }
      }
      if (camera->buffer[i] == '\n' || sc == 80) {
        //  printf("\nN!!(%02x) sc=%d\n", camera->buffer[i], sc);
        this->l[nl].line_flag = 1;  //在这里设置
        this->l[nl].len = len;
        len = 0;
        sl = 0;
        nl++;
        f = sc == 80 ? 1 : 0;
        sc = 0;
      } else {
        //   printf("Y,sc=%d\n", sc);
        this->l[nl].line[sc++].ch = camera->buffer[i];
        this->l[nl].line[sc - 1].index = i;
        len++;
        if (sc == 80) {
          this->l[nl].len = 80;
          // what the fuck?
          //     printf("\nN!!(%02x) sc=%d\n", camera->buffer[i], sc);
          nl++;
          f = sc == 80 ? 1 : 0;
          sc = 0;
          len = 0;
        }
      }
    }

    f = sc == 80 ? 1 : 0;
    sc = 0;
    if (sc == 0) {
      this->l[nl].line_flag = 1;  //在这里设置
      this->l[nl].len = len;
      this->l[nl].start_index = i;
      // this->l[nl - 1].start_index = sl;
      len = 0;
      sl = 0;
    }
    // printk("\n");
  }
  Line* getBuf() { return l; }

 private:
  Camera* camera;
  Line l[MAX_LINE];
  unsigned int wtf; // 编译器抽风了，这里不加一个变量，ny就会自动无限置1
  unsigned int ny;
  int nidx;
  void clean() {
    for (int i = 0; i < MAX_LINE; i++) {
      l[i].line_flag = 0;
      l[i].len = 0;
      l[i].start_index = 0;
      for (int j = 0; j < 80; j++) {
        l[i].line[j].index = 0;
        l[i].line[j].ch = 0;
      }
    }
  }
};
class render {
  char* buf;
  parse* p;
  Camera* camera;

 public:
  render(char* buffer, Camera* c, parse* _p) {
    buf = buffer;
    p = _p;
    camera = c;
  }
  void showAll() {
    Line* l = p->getBuf();
    char buf[90];
    int fg = 0;
    goto_xy(0, 0);
    for (int i = 0; i < MAX_LINE; i++) {
      goto_xy(0, i);
      for (int j = 0, l1 = 0; j < 80; j++) {
        printf("%c", l[i].line[j].ch == '\0' ? ' ' : l[i].line[j].ch);
        // if (!fg) {
        //   if (l[i].line[j].ch == ' ') {
        //     buf[l1] = 0;
        //     l1 = 0;
        //     if (strcmp(buf, "asm") == 0) {
        //       T_DrawBox(j - 3, i, 3, 1, 0x03);
        //     }
        //   } else {
        //     buf[l1++] = l[i].line[j].ch;
        //   }

        //   if (l[i].line[j].ch == '{' || l[i].line[j].ch == '}') {
        //     T_DrawBox(j, i, 1, 1, 0x02);
        //   }
        // } else {
        //   T_DrawBox(j, i, 1, 1, 0x04);
        // }
        // if (l[i].line[j].ch == '\"') {
        //   T_DrawBox(j, i, 1, 1, 0x04);
        //   fg = !fg;
        //   buf[0] = 0;
        //   l1 = 0;
        // }
      }
      // buf[0] = 0;
      // fg = 0;
    }
    char buf1[81] =
        "                                                                    "
        "  "
        "          ";
    char buf2[5];
    char buf0[80];
    sprintf(buf0, "COL %d LINE %d", camera->index,
            camera->y + camera->curser_pos_y + 1);
    sprintf(buf2, "%d%%",
            (int)(((float)camera->index / (float)camera->len) * 100));
    for (int i = 0; i < strlen(buf0); i++) {
      buf1[i] = buf0[i];
    }
    for (int i = 0; i < strlen(buf2); i++) {
      buf1[75 + i] = buf2[i];
    }
    setState(buf1);
    goto_xy(camera->curser_pos_x, camera->curser_pos_y);
  }
};
bool Need_Sroll(Line* l) {
  if (l[MAX_LINE - 1].line_flag == 1 || l[MAX_LINE - 1].line[0].ch != '\0') {
    return true;
  }
  return false;
}
int Show_Line_Max(Line* l) {
  int i;
  for (i = 0; i < MAX_LINE; i++) {
    if (l[i].line[0].ch == '\0' && l[i].line_flag != 1) {
      return i;
    }
  }
  return i;
}
class Note {
  Camera* camera;
  parse* p;

 public:
  int maxLine() {
    int l = 0;
    int sc = 0;
    for (int i = 0; i < camera->len; i++) {
      if (camera->buffer[i] == '\n' || sc == 80) {
        l++;
        sc = 0;
      } else {
        sc++;
      }
    }
    return l;
  }

  Note(Camera* c, parse* _p) {
    camera = c;
    p = _p;
  }
  void Insert(char ch) {
    insert_char(camera->buffer, camera->index, ch, camera);
  }
  void Delete() {
    /* 判断3“0”情况 */
    delete_char(camera->buffer, camera->index, camera);
  }
  /* 上下左右操作 */
  void up() {
    if (camera->y == 0 && camera->curser_pos_y == 0) {
      // 无法上移
      // printf("Can not up.\n");
      return;
    }
    if (camera->curser_pos_y == 0) {
      camera->y--;
    }
    p->Set();
    Line* l = p->getBuf();  // 获取当前行布局
    if (camera->curser_pos_y == 0) {
      if (l[0].len == 0) {
        camera->index = l[0].start_index;
      } else {
        camera->index = l[0].line[l[0].len - 1].index + 1;
      }
      camera->curser_pos_x = l[0].len;
      camera->curser_pos_y = 0;
    } else {
      camera->curser_pos_y--;
      if (l[camera->curser_pos_y].len == 0) {
        camera->index = l[camera->curser_pos_y].start_index;
      } else {
        camera->index = l[camera->curser_pos_y]
                            .line[l[camera->curser_pos_y].len - 1]
                            .index +
                        1;
      }
      if (camera->buffer[l[camera->curser_pos_y]
                             .line[l[camera->curser_pos_y].len - 1]
                             .index +
                         1] != '\n' &&
          l[camera->curser_pos_y].len == 80) {
        camera->curser_pos_x = l[camera->curser_pos_y].len - 1;
        camera->index--;
      } else {
        camera->curser_pos_x = l[camera->curser_pos_y].len;
      }
    }
  }
  int down() {
    Line* l;
    p->Set();
    l = p->getBuf();
    int ml = maxLine();
    if (camera->curser_pos_y != MAX_LINE - 1) {
      if (l[camera->curser_pos_y + 1].line[0].ch == '\0' &&
          l[camera->curser_pos_y + 1].line_flag == 0) {
        //	  printf("Can not Down2.\n");
        return 0;
      }
    } else {
      if (ml < (camera->y + camera->curser_pos_y) + 1) {
        // printf("Can not Down1. %d %d
        // %d\n",ml,camera->y,camera->curser_pos_y);
        //	  for(;;);
        return 0;
      }
    }
    //  printf("sure!\n");
    if (camera->curser_pos_y == MAX_LINE - 1) {
      camera->y++;
    }
    p->Set();
    l = p->getBuf();  // 获取当前行布局
    if (camera->curser_pos_y == MAX_LINE - 1) {
      if (l[MAX_LINE - 1].len == 0) {
        camera->index = l[MAX_LINE - 1].start_index;
      } else {
        camera->index = l[MAX_LINE - 1].line[l[MAX_LINE - 1].len - 1].index + 1;
      }
      camera->curser_pos_x = l[MAX_LINE - 1].len;
      camera->curser_pos_y = MAX_LINE - 1;
    } else {
      camera->curser_pos_y++;
      if (l[camera->curser_pos_y].len == 0) {
        camera->index = l[camera->curser_pos_y].start_index;
      } else {
        camera->index = l[camera->curser_pos_y]
                            .line[l[camera->curser_pos_y].len - 1]
                            .index +
                        1;
        //     printf("INDEX=%d\n", camera->index);
      }
      camera->curser_pos_x = l[camera->curser_pos_y].len;
    }
    return 1;
  }
  void left() {
    if (camera->curser_pos_x == 0) {
      up();
      Line* l = p->getBuf();

    } else {
      camera->curser_pos_x--;
      camera->index--;
    }
  }
  void right(int b) {
    // printf("this.\n");
    p->Set();
    Line* l = p->getBuf();
    if ((camera->curser_pos_x == l[camera->curser_pos_y].len && b)) {
      // printf("%d %d   s Y=%d\n", camera->curser_pos_x,
      //        l[camera->curser_pos_y].len, camera->curser_pos_y);
      // system("PAUSE");
      int ret = down();
      // system("PAUSE");

      if (ret) {
        if (b) {
          camera->curser_pos_x = 0;
          if (l[camera->curser_pos_y].len != 0) {
            camera->index = l[camera->curser_pos_y].line[0].index;
          }
        } else {
          camera->curser_pos_x = 1;
          camera->index = l[camera->curser_pos_y].start_index + 1;
        }
      }
      // asm("nop");
    } else {
      if (camera->curser_pos_x == 80) {
        camera->curser_pos_x = 1;
        if (camera->curser_pos_y == MAX_LINE - 1) {
          camera->y++;
        } else {
          camera->curser_pos_y++;
        }
      } else {
        camera->curser_pos_x++;
      }
      camera->index++;
    }
  }
};
class Editor {
 public:
  char* Main(char* filename) {
    system("cls");
    Camera* c = (Camera*)malloc(sizeof(Camera));
    c->buffer = (char*)malloc(filesize(filename) + 1000);
    char* bf2 = (char*)malloc(filesize(filename) + 1000);
    c->array_len = filesize(filename) + 1000;
    c->len = 0;

    c->y = 0;
    c->curser_pos_x = 0;
    c->curser_pos_y = 0;
    c->index = 0;
    // printf("%d %s\n",c->array_len,filename);
    if (filesize(filename) != -1) {
      api_ReadFile(filename, bf2);
      for (int i = 0, j = 0; i < filesize(filename) + 1000; i++) {
        if (bf2[i] != '\r') {
          c->buffer[j++] = bf2[i];
        }
      }
      char buffe2[100];
      sprintf(buffe2, "del %s", filename);
      system(buffe2);
    }
    mkfile(filename);

    free(bf2);
    c->len = strlen(c->buffer);
    parse* prse = new parse(c);
    prse->Set();
    Note* n = new Note(c, prse);
    Line* l = prse->getBuf();
    //  printf("%s\n", c->buffer);
    render* r = new render(c->buffer, c, prse);
    r->showAll();
    int times = 0;
    for (;;) {
      int ch = getch();
      if (ch == 0) {
        continue;
      }
      if (ch == '\n') {
        n->Insert('\n');
        n->down();
        // l = prse->getBuf();
        if (c->curser_pos_x != 0) {
          c->curser_pos_x = 0;
          c->index = l[c->curser_pos_y].line[0].index;  // Holy Fuck
        }

      } else if (ch == '\b') {
        if (c->y + c->curser_pos_x + c->curser_pos_y != 0) {
          n->left();
          n->Delete();
        }
      } else if (ch == '\t') {
        return c->buffer;
      }
      // else if (ch == 224) {
      // 	if (ch == 77) {
      // 		n->right(1);
      // 	} else if (ch == 75) {
      // 		n->left();
      // 	} else if (ch == 72) {
      // 		n->up();
      // 	} else if (ch == 80) {
      // 		n->down();
      // 	}
      else if (ch == -1) {
        n->up();
      } else if (ch == -2) {
        n->down();
      } else if (ch == -3) {
        n->left();
      } else if (ch == -4) {
        n->right(1);
      } else {
        n->Insert(ch);
        n->right(0);
      }
      prse->Set();
      r->showAll();
    }
  }
};
int mLine(char* buffer) {
  int l = 0;
  int sc = 0;
  for (int i = 0; i < strlen(buffer); i++) {
    if (buffer[i] == '\n' || sc == 80) {
      l++;
      sc = 0;
    } else {
      sc++;
    }
  }
  return l;
}
int main(int argc, char** argv) {
  if (argc == 1) {
    print("Usage:");
    print(" ");
    print(argv[0]);
    print(" ");
    print("<FileName>");
    print("\n");
    return 0;
  }
  printf("Powerint DOS Editor v0.2a\n");
  printf("We can help you write note(code)s in Powerint DOS\n");
  printf("Copyright (C) 2023 min0911_\n");
  printf("Build in %s %s\n", __DATE__, __TIME__);
  system("PAUSE");

  Editor* e = new Editor();
  char* c = e->Main(argv[1]);
  system("cls");
  char* bb = (char*)malloc(strlen(c) + 1 + mLine(c));
  for (int i = 0, j = 0; i < strlen(c); i++) {
    if (c[i] == '\n') {
      bb[j++] = '\r';
      bb[j++] = '\n';
    } else {
      bb[j++] = c[i];
    }
  }
  Edit_File(argv[1], bb, strlen(c) + mLine(c), 0);
  // char b[50];
  // printf("File:");
  // gets(b);
  // FILE *fp = fopen(b,"wb");
  // for(int i = 0;i<strlen(c);i++) {
  // 	if(c[i] == '\n') {
  // 		fputc('\r',fp);
  // 		fputc('\n',fp);
  // 	} else {
  // 		fputc(c[i],fp);
  // 	}
  // }
  return 0;
}
