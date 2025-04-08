unit Unit1;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants, System.Classes, Vcl.Graphics,
  Vcl.Controls, Vcl.Forms, Vcl.Dialogs, Vcl.StdCtrls, Vcl.ExtCtrls, System.JSON,
  Vcl.Mask;

type
  TForm1 = class(TForm)
    OpenDialog1: TOpenDialog;
    Image1: TImage;
    Panel1: TPanel;
    ButtonprocessGerberJSON: TButton;
    ButtonProcessGerber: TButton;
    CheckBoxDebug: TCheckBox;
    MaskEditDpi: TMaskEdit;
    procedure ButtonprocessGerberJSONClick(Sender: TObject);
    procedure ButtonProcessGerberClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
  private
    function GetErrorMessageByCode(code: Integer): string;
    function GetImageDPI: Double;
  public
  end;

var
  Form1: TForm1;

implementation

{$R *.dfm}

{$IFDEF CPUX64}
const
  GerberDll = 'gerb2img_x64.dll';
  GerberDebugDll = 'gerb2img_x64_debug.dll';
{$ELSE}
const
  GerberDll = 'gerb2img_x32.dll';
  GerberDebugDll = 'gerb2img_x32_debug.dll';
{$ENDIF}

// === ПСЕВДОНИМЫ ДЛЯ ФУНКЦИЙ В ЗАВИСИМОСТИ ОТ DLL ===

// Для обычной библиотеки
function processGerberRelease(imageDPI: Double;
                              optGrowUnitsMillimeters: Boolean;
                              optBoarderUnitsMillimeters: Boolean;
                              optBoarder: Double;
                              optInvertPolarity: Boolean;
                              rowsPerStrip: Cardinal;
                              optGrowSize: Double;
                              optScaleX: Double;
                              optScaleY: Double;
                              outputFilename: PAnsiChar;
                              inputFilename: PAnsiChar): Integer; stdcall;
                              external GerberDll name 'processGerber';

function processGerberJSONRelease(const json: PAnsiChar): Integer; stdcall;
                              external GerberDll name 'processGerberJSON';

// Для отладочной библиотеки
function processGerberDebug(imageDPI: Double;
                            optGrowUnitsMillimeters: Boolean;
                            optBoarderUnitsMillimeters: Boolean;
                            optBoarder: Double;
                            optInvertPolarity: Boolean;
                            rowsPerStrip: Cardinal;
                            optGrowSize: Double;
                            optScaleX: Double;
                            optScaleY: Double;
                            outputFilename: PAnsiChar;
                            inputFilename: PAnsiChar): Integer; stdcall;
                            external GerberDebugDll name 'processGerber';

function processGerberJSONDebug(const json: PAnsiChar): Integer; stdcall;
                            external GerberDebugDll name 'processGerberJSON';

// === ПОЛУЧЕНИЕ DPI ===
function TForm1.GetImageDPI: Double;
begin
  if TryStrToFloat(MaskEditDpi.Text, Result) then
  begin
    if Result <= 0 then Result := 1024;
  end
  else
    Result := 1024;
end;

// === КОД ОШИБКИ ===
function TForm1.GetErrorMessageByCode(code: Integer): string;
begin
  case code of
    0:    Result := 'Успешно.';
    2:    Result := 'Невозможно открыть файл.';
    3:    Result := 'Ошибка обработки Gerber.';
    4:    Result := 'Некорректные параметры.';
    5:    Result := 'Нет изображения для обработки.';
    6:    Result := 'Ошибка выделения памяти.';
    7:    Result := 'Ошибка создания выходного файла.';
    8:    Result := 'Ошибка обработки JSON.';
    9999: Result := 'Неизвестная ошибка.';
  else
    Result := 'Неизвестная ошибка.';
  end;
  Result := Result + ' (Код: ' + IntToStr(code) + ')';
end;

// === ОБРАБОТКА Gerber (обычная) ===
procedure TForm1.ButtonProcessGerberClick(Sender: TObject);
var
  inputFilePath, outputFilePath: string;
  resultCode: Integer;
begin
  if OpenDialog1.Execute then
  begin
    inputFilePath := OpenDialog1.FileName;
    outputFilePath := 'OUTPUT.bmp';

    // Устанавливаем курсор в режим "ожидание"
    Screen.Cursor := crHourGlass;

    try
      if CheckBoxDebug.Checked then
      begin
        resultCode := processGerberDebug(
          GetImageDPI, False, False, 0, False, 512, 0, 1, 1,
          PAnsiChar(AnsiString(outputFilePath)),
          PAnsiChar(AnsiString(inputFilePath))
        );
      end
      else
      begin
        resultCode := processGerberRelease(
          GetImageDPI, False, False, 0, False, 512, 0, 1, 1,
          PAnsiChar(AnsiString(outputFilePath)),
          PAnsiChar(AnsiString(inputFilePath))
        );
      end;

      if resultCode = 0 then
      begin
        Image1.Picture.LoadFromFile(outputFilePath);
        ShowMessage('Конвертация успешно завершена!');
      end
      else
      begin
        ShowMessage(GetErrorMessageByCode(resultCode));
      end;
    finally
      // Возвращаем курсор в исходное состояние
      Screen.Cursor := crDefault;
    end;
  end;
end;

// === ОБРАБОТКА Gerber через JSON ===
procedure TForm1.ButtonprocessGerberJSONClick(Sender: TObject);
var
  jsonString, inputFilePath, escapedInputPath, outputFilePath: string;
  resultCode: Integer;
begin
  if OpenDialog1.Execute then
  begin
    inputFilePath := OpenDialog1.FileName;
    escapedInputPath := StringReplace(inputFilePath, '\', '\\', [rfReplaceAll]);
    outputFilePath := 'OUTPUT.bmp';

    jsonString := '{' + sLineBreak +
                  '  "imageDPI": ' + FloatToStr(GetImageDPI) + ',' + sLineBreak +
                  '  "optGrowUnitsMillimeters": false,' + sLineBreak +
                  '  "optBoarderUnitsMillimeters": false,' + sLineBreak +
                  '  "optBoarder": 0,' + sLineBreak +
                  '  "optInvertPolarity": true,' + sLineBreak +
                  '  "rowsPerStrip": 512,' + sLineBreak +
                  '  "optGrowSize": 0,' + sLineBreak +
                  '  "optScaleX": 1,' + sLineBreak +
                  '  "optScaleY": 1,' + sLineBreak +
                  '  "outputFilename": "' + outputFilePath + '",' + sLineBreak +
                  '  "inputFilename": "' + escapedInputPath + '"' + sLineBreak +
                  '}';

    // Устанавливаем курсор в режим "ожидание"
    Screen.Cursor := crHourGlass;

    try
      if CheckBoxDebug.Checked then
        resultCode := processGerberJSONDebug(PAnsiChar(AnsiString(jsonString)))
      else
        resultCode := processGerberJSONRelease(PAnsiChar(AnsiString(jsonString)));

      if resultCode = 0 then
      begin
        Image1.Picture.LoadFromFile(outputFilePath);
        ShowMessage('Конвертация успешно завершена!');
      end
      else
      begin
        ShowMessage(GetErrorMessageByCode(resultCode));
      end;
    finally
      // Возвращаем курсор в исходное состояние
      Screen.Cursor := crDefault;
    end;
  end;
end;


procedure TForm1.FormCreate(Sender: TObject);
begin
  MaskEditDpi.Text := '1024';
end;

end.

