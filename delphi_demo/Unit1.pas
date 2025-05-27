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
    ButtonProcessExcellon: TButton;
    ButtonProcessExcellonJSON: TButton;
    GroupBox1: TGroupBox;
    GerberCheckBoxInvertPolarity: TCheckBox;
    GerberEditScaleX: TEdit;
    GerberLabelScaleX: TLabel;
    GerberLabelScale: TLabel;
    GerberEditScaleY: TEdit;
    Edit1: TEdit;
    GerberCheckBoxBorderMillimeters: TCheckBox;
    GerberEditGrowSize: TEdit;
    GerberCheckBoxGrowMillimeters: TCheckBox;
    GroupBox2: TGroupBox;
    ExcellonCheckBoxInvertPolarity: TCheckBox;
    ExcellonLabelScaleX: TLabel;
    ExcellonEditScaleX: TEdit;
    ExcellonLabelScaleY: TLabel;
    ExcellonEditScaleY: TEdit;
    ExcellonCheckBoxUnitsMillimeters: TCheckBox;
    ExcellonLabelBorder: TLabel;
    ExcellonEditBorder: TEdit;
    ExcellonLabelGrowSize: TLabel;
    ExcellonEditGrowSize: TEdit;
    ExcellonCheckBoxUniformDrills: TCheckBox;
    ExcellonCheckBoxUniformDrillsMillimeters: TCheckBox;
    ExcellonEditUniformDrillDiameter: TEdit;
    procedure ButtonprocessGerberJSONClick(Sender: TObject);
    procedure ButtonProcessGerberClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure ButtonProcessExcellonClick(Sender: TObject);
    procedure ButtonProcessExcellonJSONClick(Sender: TObject);
    procedure Panel3Click(Sender: TObject);
    procedure ExcellonEditGrowSizeChange(Sender: TObject);
  private
    function GetErrorMessageByCode(code: Integer): string;
    function GetImageDPI: Double;
    // Герберовские функции получения параметров
    function GetGerberScaleX: Double;
    function GetGerberScaleY: Double;
    function GetGerberBorder: Double;
    function GetGerberGrowSize: Double;
    function GetGerberBorderMillimeters: Boolean;
    function GetGerberGrowMillimeters: Boolean;
    function GetGerberInvertPolarity: Boolean;
    // Экселлоновские функции получения параметров
    function GetExcellonScaleX: Double;
    function GetExcellonScaleY: Double;
    function GetExcellonBorder: Double;
    function GetExcellonGrowSize: Double;
    function GetExcellonUnitsMillimeters: Boolean;
    function GetExcellonInvertPolarity: Boolean;
    function GetExcellonUniformDrills: Boolean;
    function GetExcellonUniformDrillsMillimeters: Boolean;
    function GetExcellonUniformDrillDiameter: Double;
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

// === Функции для обработки Gerber (релиз) ===

// Процесс обработки Gerber
function processGerberRelease(imageDPI: Double;
                              optGrowUnitsMillimeters: Boolean;
                              optBoarderUnitsMillimeters: Boolean;
                              optBoarder: Double;
                              optInvertPolarity: Boolean;
                              optGrowSize: Double;
                              optScaleX: Double;
                              optScaleY: Double;
                              outputFilename: PAnsiChar;
                              inputFilename: PAnsiChar): Integer; stdcall;
                              external GerberDll name 'processGerber';

function processGerberJSONRelease(const json: PAnsiChar): Integer; stdcall;
                              external GerberDll name 'processGerberJSON';

// === Функции для обработки Gerber (отладка) ===
function processGerberDebug(imageDPI: Double;
                            optGrowUnitsMillimeters: Boolean;
                            optBoarderUnitsMillimeters: Boolean;
                            optBoarder: Double;
                            optInvertPolarity: Boolean;
                            optGrowSize: Double;
                            optScaleX: Double;
                            optScaleY: Double;
                            outputFilename: PAnsiChar;
                            inputFilename: PAnsiChar): Integer; stdcall;
                            external GerberDebugDll name 'processGerber';

function processGerberJSONDebug(const json: PAnsiChar): Integer; stdcall;
                            external GerberDebugDll name 'processGerberJSON';

// === Функции для обработки Excellon (релиз) ===
function processExcellonRelease(imageDPI: Double;
                             unitsMillimeters: Boolean;
                             optBoarder: Double;
                             optInvertPolarity: Boolean;
                             optGrowSize: Double;
                             optScaleX: Double;
                             optScaleY: Double;
                             uniformDrills: Boolean;
                             uniformDrillsMillimeters: Boolean;
                             uniformDrillDiameter: Double;
                             outputFilename: PAnsiChar;
                             inputFilename: PAnsiChar): Integer; stdcall;
                             external GerberDll name 'processExcellon';

function processExcellonJSONRelease(const json: PAnsiChar): Integer; stdcall;
                             external GerberDll name 'processExcellonJSON';

// === Функции для обработки Excellon (отладка) ===
function processExcellonDebug(imageDPI: Double;
                           unitsMillimeters: Boolean;
                           optBoarder: Double;
                           optInvertPolarity: Boolean;
                           optGrowSize: Double;
                           optScaleX: Double;
                           optScaleY: Double;
                           uniformDrills: Boolean;
                           uniformDrillsMillimeters: Boolean;
                           uniformDrillDiameter: Double;
                           outputFilename: PAnsiChar;
                           inputFilename: PAnsiChar): Integer; stdcall;
                           external GerberDebugDll name 'processExcellon';

function processExcellonJSONDebug(const json: PAnsiChar): Integer; stdcall;
                           external GerberDebugDll name 'processExcellonJSON';

// === Получение DPI ===
function TForm1.GetImageDPI: Double;
begin
  if TryStrToFloat(MaskEditDpi.Text, Result) then
  begin
    if Result <= 0 then Result := 1024;
  end
  else
    Result := 1024;
end;

procedure TForm1.Panel3Click(Sender: TObject);
begin

end;

// === Код ошибки ===
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
    9:    Result := 'Ошибка обработки Excellon.';
    10:   Result := 'Функциональность не реализована.';
    9999: Result := 'Неизвестная ошибка.';
  else
    Result := 'Неизвестная ошибка.';
  end;
  Result := Result + ' (код: ' + IntToStr(code) + ')';
end;

// === Обработка Gerber (кнопка) ===
procedure TForm1.ButtonProcessGerberClick(Sender: TObject);
var
  inputFilePath, outputFilePath: string;
  resultCode: Integer;
begin
  if OpenDialog1.Execute then
  begin
    inputFilePath := OpenDialog1.FileName;
    outputFilePath := 'OUTPUT.bmp';

    // Инициализация курсор в форму "песочные часы"
    Screen.Cursor := crHourGlass;

    try
      if CheckBoxDebug.Checked then
      begin
        resultCode := processGerberDebug(
          GetImageDPI, 
          GetGerberGrowMillimeters, 
          GetGerberBorderMillimeters, 
          GetGerberBorder, 
          GetGerberInvertPolarity, 
          GetGerberGrowSize, 
          GetGerberScaleX, 
          GetGerberScaleY,
          PAnsiChar(AnsiString(outputFilePath)),
          PAnsiChar(AnsiString(inputFilePath))
        );
      end
      else
      begin
        resultCode := processGerberRelease(
          GetImageDPI, 
          GetGerberGrowMillimeters, 
          GetGerberBorderMillimeters, 
          GetGerberBorder, 
          GetGerberInvertPolarity, 
          GetGerberGrowSize, 
          GetGerberScaleX, 
          GetGerberScaleY,
          PAnsiChar(AnsiString(outputFilePath)),
          PAnsiChar(AnsiString(inputFilePath))
        );
      end;

      if resultCode = 0 then
      begin
        Image1.Picture.LoadFromFile(outputFilePath);
        ShowMessage('Изображение успешно обработано!');
      end
      else
      begin
        ShowMessage(GetErrorMessageByCode(resultCode));
      end;
    finally
      // Возвращаем курсор к обычному состоянию
      Screen.Cursor := crDefault;
    end;
  end;
end;

// === Обработка Gerber через JSON ===
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
                  '  "optGrowUnitsMillimeters": ' + LowerCase(BoolToStr(GetGerberGrowMillimeters, True)) + ',' + sLineBreak +
                  '  "optBoarderUnitsMillimeters": ' + LowerCase(BoolToStr(GetGerberBorderMillimeters, True)) + ',' + sLineBreak +
                  '  "optBoarder": ' + FloatToStr(GetGerberBorder) + ',' + sLineBreak +
                  '  "optInvertPolarity": ' + LowerCase(BoolToStr(GetGerberInvertPolarity, True)) + ',' + sLineBreak +
                  '  "optGrowSize": ' + FloatToStr(GetGerberGrowSize) + ',' + sLineBreak +
                  '  "optScaleX": ' + FloatToStr(GetGerberScaleX) + ',' + sLineBreak +
                  '  "optScaleY": ' + FloatToStr(GetGerberScaleY) + ',' + sLineBreak +
                  '  "outputFilename": "' + outputFilePath + '",' + sLineBreak +
                  '  "inputFilename": "' + escapedInputPath + '"' + sLineBreak +
                  '}';

    // Инициализация курсор в форму "песочные часы"
    Screen.Cursor := crHourGlass;

    try
      if CheckBoxDebug.Checked then
        resultCode := processGerberJSONDebug(PAnsiChar(AnsiString(jsonString)))
      else
        resultCode := processGerberJSONRelease(PAnsiChar(AnsiString(jsonString)));

      if resultCode = 0 then
      begin
        Image1.Picture.LoadFromFile(outputFilePath);
        ShowMessage('Изображение успешно обработано!');
      end
      else
      begin
        ShowMessage(GetErrorMessageByCode(resultCode));
      end;
    finally
      // Возвращаем курсор к обычному состоянию
      Screen.Cursor := crDefault;
    end;
  end;
end;

procedure TForm1.ExcellonEditGrowSizeChange(Sender: TObject);
begin

end;

// === Обработка Excellon (кнопка) ===
procedure TForm1.ButtonProcessExcellonClick(Sender: TObject);
var
  inputFilePath, outputFilePath: string;
  resultCode: Integer;
begin
  if OpenDialog1.Execute then
  begin
    inputFilePath := OpenDialog1.FileName;
    outputFilePath := 'OUTPUT.bmp';

    // Инициализация курсор в форму "песочные часы"
    Screen.Cursor := crHourGlass;

    try
      if CheckBoxDebug.Checked then
      begin
        resultCode := processExcellonDebug(
          GetImageDPI, 
          GetExcellonUnitsMillimeters, 
          GetExcellonBorder, 
          GetExcellonInvertPolarity, 
          GetExcellonGrowSize, 
          GetExcellonScaleX, 
          GetExcellonScaleY,
          GetExcellonUniformDrills,
          GetExcellonUniformDrillsMillimeters,
          GetExcellonUniformDrillDiameter,
          PAnsiChar(AnsiString(outputFilePath)),
          PAnsiChar(AnsiString(inputFilePath))
        );
      end
      else
      begin
        resultCode := processExcellonRelease(
          GetImageDPI, 
          GetExcellonUnitsMillimeters, 
          GetExcellonBorder, 
          GetExcellonInvertPolarity, 
          GetExcellonGrowSize, 
          GetExcellonScaleX, 
          GetExcellonScaleY,
          GetExcellonUniformDrills,
          GetExcellonUniformDrillsMillimeters,
          GetExcellonUniformDrillDiameter,
          PAnsiChar(AnsiString(outputFilePath)),
          PAnsiChar(AnsiString(inputFilePath))
        );
      end;

      if resultCode = 0 then
      begin
        Image1.Picture.LoadFromFile(outputFilePath);
        ShowMessage('Изображение успешно обработано!');
      end
      else
      begin
        ShowMessage(GetErrorMessageByCode(resultCode));
      end;
    finally
      // Возвращаем курсор к обычному состоянию
      Screen.Cursor := crDefault;
    end;
  end;
end;

// === Обработка Excellon через JSON ===
procedure TForm1.ButtonProcessExcellonJSONClick(Sender: TObject);
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
                  '  "unitsMillimeters": ' + LowerCase(BoolToStr(GetExcellonUnitsMillimeters, True)) + ',' + sLineBreak +
                  '  "optBoarder": ' + FloatToStr(GetExcellonBorder) + ',' + sLineBreak +
                  '  "optInvertPolarity": ' + LowerCase(BoolToStr(GetExcellonInvertPolarity, True)) + ',' + sLineBreak +
                  '  "optGrowSize": ' + FloatToStr(GetExcellonGrowSize) + ',' + sLineBreak +
                  '  "optScaleX": ' + FloatToStr(GetExcellonScaleX) + ',' + sLineBreak +
                  '  "optScaleY": ' + FloatToStr(GetExcellonScaleY) + ',' + sLineBreak +
                  '  "uniformDrills": ' + LowerCase(BoolToStr(GetExcellonUniformDrills, True)) + ',' + sLineBreak +
                  '  "uniformDrillsMillimeters": ' + LowerCase(BoolToStr(GetExcellonUniformDrillsMillimeters, True)) + ',' + sLineBreak +
                  '  "uniformDrillDiameter": ' + FloatToStr(GetExcellonUniformDrillDiameter) + ',' + sLineBreak +
                  '  "outputFilename": "' + outputFilePath + '",' + sLineBreak +
                  '  "inputFilename": "' + escapedInputPath + '"' + sLineBreak +
                  '}';

    // Инициализация курсор в форму "песочные часы"
    Screen.Cursor := crHourGlass;

    try
      if CheckBoxDebug.Checked then
        resultCode := processExcellonJSONDebug(PAnsiChar(AnsiString(jsonString)))
      else
        resultCode := processExcellonJSONRelease(PAnsiChar(AnsiString(jsonString)));

      if resultCode = 0 then
      begin
        Image1.Picture.LoadFromFile(outputFilePath);
        ShowMessage('Изображение успешно обработано!');
      end
      else
      begin
        ShowMessage(GetErrorMessageByCode(resultCode));
      end;
    finally
      // Возвращаем курсор к обычному состоянию
      Screen.Cursor := crDefault;
    end;
  end;
end;

procedure TForm1.FormCreate(Sender: TObject);
begin
  MaskEditDpi.Text := '1024';
  
  // Инициализация контролов для Gerber
  GerberEditScaleX.Text := '1.0';
  GerberEditScaleY.Text := '1.0';
  Edit1.Text := '0.0';
  GerberEditGrowSize.Text := '0.0';
  GerberCheckBoxInvertPolarity.Checked := False;
  GerberCheckBoxBorderMillimeters.Checked := False;
  GerberCheckBoxGrowMillimeters.Checked := False;
  
  // Инициализация контролов для Excellon
  ExcellonEditScaleX.Text := '1.0';
  ExcellonEditScaleY.Text := '1.0';
  ExcellonEditBorder.Text := '0.0';
  ExcellonEditGrowSize.Text := '0.0';
  ExcellonEditUniformDrillDiameter.Text := '0.0';
  ExcellonCheckBoxInvertPolarity.Checked := False;
  ExcellonCheckBoxUnitsMillimeters.Checked := False;
  ExcellonCheckBoxUniformDrills.Checked := False;
  ExcellonCheckBoxUniformDrillsMillimeters.Checked := False;
end;

// Реализация вспомогательных функций для Gerber
function TForm1.GetGerberScaleX: Double;
begin
  if not TryStrToFloat(GerberEditScaleX.Text, Result) then
    Result := 1.0;
  if Result <= 0 then Result := 1.0;
end;

function TForm1.GetGerberScaleY: Double;
begin
  if not TryStrToFloat(GerberEditScaleY.Text, Result) then
    Result := 1.0;
  if Result <= 0 then Result := 1.0;
end;

function TForm1.GetGerberBorder: Double;
begin
  if not TryStrToFloat(Edit1.Text, Result) then
    Result := 0.0;
  if Result < 0 then Result := 0.0;
end;

function TForm1.GetGerberGrowSize: Double;
begin
  if not TryStrToFloat(GerberEditGrowSize.Text, Result) then
    Result := 0.0;
end;

function TForm1.GetGerberBorderMillimeters: Boolean;
begin
  Result := GerberCheckBoxBorderMillimeters.Checked;
end;

function TForm1.GetGerberGrowMillimeters: Boolean;
begin
  Result := GerberCheckBoxGrowMillimeters.Checked;
end;

function TForm1.GetGerberInvertPolarity: Boolean;
begin
  Result := GerberCheckBoxInvertPolarity.Checked;
end;

// Реализация вспомогательных функций для Excellon
function TForm1.GetExcellonScaleX: Double;
begin
  if not TryStrToFloat(ExcellonEditScaleX.Text, Result) then
    Result := 1.0;
  if Result <= 0 then Result := 1.0;
end;

function TForm1.GetExcellonScaleY: Double;
begin
  if not TryStrToFloat(ExcellonEditScaleY.Text, Result) then
    Result := 1.0;
  if Result <= 0 then Result := 1.0;
end;

function TForm1.GetExcellonBorder: Double;
begin
  if not TryStrToFloat(ExcellonEditBorder.Text, Result) then
    Result := 0.0;
  if Result < 0 then Result := 0.0;
end;

function TForm1.GetExcellonGrowSize: Double;
begin
  if not TryStrToFloat(ExcellonEditGrowSize.Text, Result) then
    Result := 0.0;
end;

function TForm1.GetExcellonUnitsMillimeters: Boolean;
begin
  Result := ExcellonCheckBoxUnitsMillimeters.Checked;
end;

function TForm1.GetExcellonInvertPolarity: Boolean;
begin
  Result := ExcellonCheckBoxInvertPolarity.Checked;
end;

function TForm1.GetExcellonUniformDrills: Boolean;
begin
  Result := ExcellonCheckBoxUniformDrills.Checked;
end;

function TForm1.GetExcellonUniformDrillsMillimeters: Boolean;
begin
  Result := ExcellonCheckBoxUniformDrillsMillimeters.Checked;
end;

function TForm1.GetExcellonUniformDrillDiameter: Double;
begin
  if not TryStrToFloat(ExcellonEditUniformDrillDiameter.Text, Result) then
    Result := 0.0;
  if Result < 0 then Result := 0.0;
end;

end.

