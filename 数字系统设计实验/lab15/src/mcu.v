//  mcu.v  —  音乐播放器主控单元（顶层）
//    - controller : 状态机
//    - song_counter : 2位二进制计数器
module mcu (
    input  wire       clk, reset, play_pause, next, song_done,
    output wire       play, reset_play,
    output wire [1:0] song
);
    wire next_song;
    // 实例化控制器
    controller u_ctrl (
        .clk(clk), .reset(reset), .play_pause(play_pause), .next(next),
        .song_done(song_done), .play(play), .reset_play(reset_play), .next_song(next_song)
    );
    // 实例化歌曲序号计数器
    song_counter u_cnt (
        .clk(clk), .clr(reset), .en(next_song), .q(song)
    );
endmodule

//  controller.v  —  有限状态机（状态：复位、暂停、播放、下一首）
module controller (
    input  wire clk, reset, play_pause, next, song_done,
    output reg  play, reset_play, next_song
);
    localparam [1:0] S_RESET = 2'd0, S_PAUSE = 2'd1, S_PLAY  = 2'd2, S_NEXT  = 2'd3;
    reg [1:0] state, next_state;
    // 状态寄存器
    always @(posedge clk) begin
        if (reset) state <= S_RESET;
        else state <= next_state;
    end
    // 次态逻辑
    always @(*) begin
        next_state = state;
        case (state)
            S_RESET: next_state = S_PAUSE;
            S_PAUSE: if (play_pause) next_state = S_PLAY;
                     else if (next) next_state = S_NEXT;
                     else next_state = S_PAUSE;
            S_PLAY:  if (play_pause) next_state = S_PAUSE;
                     else if (next) next_state = S_NEXT;
                     else if (song_done) next_state = S_RESET;
                     else next_state = S_PLAY;
            S_NEXT:  next_state = S_PLAY;
            default: next_state = S_RESET;
        endcase
    end
    // 输出逻辑
    always @(*) begin
        play = 1'b0; reset_play = 1'b0; next_song = 1'b0;
        case (state)
            S_RESET: reset_play = 1'b1;
            S_PAUSE: ;
            S_PLAY:  play = 1'b1;
            S_NEXT:  begin next_song = 1'b1; reset_play = 1'b1; end
            default: reset_play = 1'b1;
        endcase
    end
endmodule

//  song_counter.v  —  2位二进制加法计数器（0→1→2→3→0）
module song_counter (
    input  wire       clk, clr, en,
    output reg  [1:0] q
);
    always @(posedge clk) begin
        if (clr) q <= 2'b00;
        else if (en) q <= q + 2'b01;
    end
endmodule