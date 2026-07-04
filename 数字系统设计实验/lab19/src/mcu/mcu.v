`timescale 1ns / 1ps

// mcu (主控单元顶层)
module mcu(
    input  wire       clk,
    input  wire       reset,
    input  wire       play_pause,
    input  wire       next,
    input  wire       song_done,
    output wire       play,
    output wire       reset_play,
    output wire [1:0] song
);

    wire next_song;

    // 实例化控制器
    controller u_controller(
        .clk        (clk),
        .reset      (reset),
        .play_pause (play_pause),
        .next       (next),
        .song_done  (song_done),
        .play       (play),
        .reset_play (reset_play),
        .next_song  (next_song)
    );

    // 实例化歌曲计数器
    song_counter u_song_counter(
        .clk        (clk),
        .reset      (reset),
        .next_song  (next_song),
        .song       (song)
    );

endmodule


// 子模块: controller (有限状态机)

module controller(
    input  wire clk,
    input  wire reset,
    input  wire play_pause,
    input  wire next,
    input  wire song_done,
    output reg  play,
    output reg  reset_play,
    output reg  next_song
);

    // 4个状态
    parameter RESET = 2'b00, PAUSE = 2'b01, PLAY = 2'b10, NEXT = 2'b11;
    reg [1:0] state, next_state;

    // 1. 状态寄存器
    always @(posedge clk) begin
        if (reset)
            state <= RESET;
        else
            state <= next_state;
    end

    // 2. 次态逻辑 
    always @(*) begin
        case (state)
            RESET: next_state = PAUSE;
            
            PAUSE: begin
                if (play_pause) next_state = PLAY;
                else if (next)  next_state = NEXT;
                else            next_state = PAUSE;
            end
            
            PLAY: begin
                if (play_pause)      next_state = PAUSE;
                else if (next)       next_state = NEXT;
                else if (song_done)  next_state = RESET;
                else                 next_state = PLAY;
            end
            
            // 切歌(NEXT)后，无条件进入播放(PLAY)
            NEXT: next_state = PLAY;   
            
            default: next_state = RESET;
        endcase
    end

    // 3. 输出逻辑
    always @(*) begin
        case (state)
            RESET: begin play = 0; reset_play = 1; next_song = 0; end
            PAUSE: begin play = 0; reset_play = 0; next_song = 0; end
            PLAY:  begin play = 1; reset_play = 0; next_song = 0; end
            NEXT:  begin play = 0; reset_play = 1; next_song = 1; end
            default: begin play = 0; reset_play = 0; next_song = 0; end
        endcase
    end

endmodule


// 子模块: song_counter (2位歌曲计数器)

module song_counter(
    input  wire       clk,
    input  wire       reset,
    input  wire       next_song,
    output reg  [1:0] song
);

    always @(posedge clk) begin
        if (reset)
            song <= 2'b00;
        else if (next_song)
            song <= song + 1'b1;
    end

endmodule